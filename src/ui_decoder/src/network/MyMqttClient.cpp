// ============================================================
// bridge_mqtt_client.cpp -- MyMqttClient 实现
//
// 通信流程:
//   1) 自定义客户端将 Protobuf 对象序列化为二进制
//   2) 调用 publish() 把二进制发到对应的 topic
//   3) 订阅方收到二进制后,用 Protobuf 反序列化
// ============================================================

#include "MyMqttClient.h"

#include <mqtt/client.h>
#include <mqtt/callback.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {
std::mutex g_logMutex;

void log_info(const std::string& msg) {
    std::lock_guard<std::mutex> lk(g_logMutex);
    std::cerr << "[MyMqttClient] " << msg << std::endl;
}

// ------------------------------------------------------------
// 连接状态回调: 自动重连成功后触发重订阅。
// 通过持有弱指针避免回调对象在 MyMqttClient 析构后仍被 Paho 调用。
// ------------------------------------------------------------
class ClientCallback : public mqtt::callback {
public:
    explicit ClientCallback(MyMqttClient* owner) : m_owner(owner) {}

    void connected(const std::string& /*cause*/) override {
        if (m_owner) {
            m_owner->resubscribeAll();
        }
    }

    void connection_lost(const std::string& cause) override {
        log_info("connection lost: " + cause);
    }

private:
    MyMqttClient* m_owner;
};
} // namespace

// ============================================================
// 构造 / 析构
// ============================================================

MyMqttClient::MyMqttClient(uint32_t robot_id,
                           const std::string& ip,
                           int port)
    : m_robotId(robot_id), m_ip(ip), m_port(port)
{
    // ------------------------------------------------------------
    // clientID: 填入所连接的机器人 ID 编号
    // 与 MQTTX / broker 侧保持一致, 直接使用机器人编号字符串,
    // 例如 robot_id=1 时 clientID 为 "1"。
    // 注意: 不能加 "robot_" 之类的自定义前缀, broker 会校验 clientID,
    // 不匹配时返回 CONNACK 5 (not authorized), 连接被拒绝。
    // ------------------------------------------------------------
    m_clientId = std::to_string(m_robotId);

    // 关键: Paho C++ 的 MQTT 版本是在构造 client 时通过 create_options 指定的,
    // 而不是 connect_options.set_mqtt_version()。若两者不一致,连接会报
    // "Client created for another version of MQTT"(-16)。
    // 这里先按 broker 侧实际使用的 MQTT 5.0 创建,Connect() 失败时会回退 3.1.1。
    recreateClient(MQTTVERSION_5);
}

void MyMqttClient::recreateClient(int mqtt_version) {
    const std::string serverURI = "tcp://" + m_ip + ":" + std::to_string(m_port);

    // 必须先析构旧 client(它内部持有旧 callback 的引用),再替换 callback,
    // 保证 callback 生命周期始终覆盖 client。
    m_client.reset();
    m_callback = std::make_shared<ClientCallback>(this);
    m_client = std::make_shared<mqtt::client>(serverURI, m_clientId,
                                              mqtt::create_options(mqtt_version));
    m_client->set_callback(*m_callback);
}

MyMqttClient::~MyMqttClient() {
    StopReceiveLoop();
    try {
        if (m_client && m_client->is_connected()) {
            m_client->disconnect();
        }
    } catch (...) {
        // 析构阶段吞掉所有异常
    }
    // m_callback 必须比 m_client 更晚析构 (成员声明顺序已保证),
    // 这里不再显式操作。
}

// ============================================================
// 连接 / 断开
// ============================================================

bool MyMqttClient::Connect() {
    std::lock_guard<std::mutex> lk(m_clientMutex);
    if (!m_client) {
        return false;
    }
    if (m_client->is_connected()) {
        return true;  // 已连接, 幂等
    }

    auto build_options = [](int mqtt_version) {
        mqtt::connect_options opts;
        opts.set_keep_alive_interval(20);
        opts.set_clean_session(true);
        opts.set_automatic_reconnect(true);
        opts.set_connect_timeout(std::chrono::seconds(5));
        opts.set_mqtt_version(mqtt_version);
        return opts;
    };

    // 先尝试 MQTT 5.0 (broker 侧 MQTTX 使用 5.0)。若 broker 只支持
    // 3.1.1/3.1,则用对应版本重建 client 后重试。
    try {
        recreateClient(MQTTVERSION_5);
        m_client->connect(build_options(MQTTVERSION_5));
        if (m_client->is_connected()) {
            log_info("connected with MQTT 5.0, clientID=" + m_clientId);
            return true;
        }
    } catch (const std::exception& e) {
        log_info(std::string("connect v5 failed: ") + e.what());
    }

    try {
        // 默认版本从 3.1.1 开始, 失败后自动回退 3.1。
        recreateClient(MQTTVERSION_DEFAULT);
        m_client->connect(build_options(MQTTVERSION_DEFAULT));
        if (m_client->is_connected()) {
            log_info("connected with MQTT default (3.1.1/3.1), clientID=" + m_clientId);
            return true;
        }
    } catch (const std::exception& e) {
        log_info(std::string("connect default failed: ") + e.what());
    }

    log_info(std::string("connect failed: ") + m_ip + ":" + std::to_string(m_port));
    return false;
}

void MyMqttClient::Disconnect() {
    StopReceiveLoop();
    std::lock_guard<std::mutex> lk(m_clientMutex);
    try {
        if (m_client && m_client->is_connected()) {
            m_client->disconnect();
        }
    } catch (const std::exception& e) {
        log_info(std::string("disconnect failed: ") + e.what());
    }
}

bool MyMqttClient::IsConnected() const {
    std::lock_guard<std::mutex> lk(m_clientMutex);
    return m_client && m_client->is_connected();
}

// ============================================================
// 发布:Protobuf 二进制 / 字符串
// ============================================================

void MyMqttClient::PublishProto(const std::string& topic,
                                const std::vector<uint8_t>& proto_bytes,
                                MqttQosLevel level,
                                bool retain) {
    std::lock_guard<std::mutex> lk(m_clientMutex);
    if (!m_client || !m_client->is_connected()) {
        log_info("PublishProto: not connected, drop message topic=" + topic);
        return;
    }
    if (proto_bytes.empty()) {
        return;  // 空 payload 不发布
    }
    try {
        // mqtt::binary_ref 期望 const char*;protobuf 的字节流是 uint8_t
        mqtt::binary_ref bin(reinterpret_cast<const char*>(proto_bytes.data()),
                             proto_bytes.size());
        auto msg = mqtt::message::create(topic, bin,
                                         static_cast<int>(level), retain);
        m_client->publish(msg);
    } catch (const std::exception& e) {
        log_info(std::string("publish proto failed: ") + e.what());
    }
}

void MyMqttClient::PublishString(const std::string& topic,
                                 const std::string& message,
                                 MqttQosLevel level,
                                 bool retain) {
    std::lock_guard<std::mutex> lk(m_clientMutex);
    if (!m_client || !m_client->is_connected()) {
        log_info("PublishString: not connected, drop message topic=" + topic);
        return;
    }
    try {
        auto msg = mqtt::message::create(topic, message,
                                         static_cast<int>(level), retain);
        m_client->publish(msg);
    } catch (const std::exception& e) {
        log_info(std::string("publish string failed: ") + e.what());
    }
}

// ============================================================
// 订阅
// ============================================================

void MyMqttClient::Subscribe(const std::string& topic, MqttQosLevel level) {
    {
        std::lock_guard<std::mutex> lk(m_subscriptionMutex);
        m_subscriptions[topic] = static_cast<int>(level);
    }

    std::lock_guard<std::mutex> lk(m_clientMutex);
    if (!m_client || !m_client->is_connected()) {
        log_info("Subscribe: not connected yet, queued topic=" + topic);
        return;  // 连接成功后由 resubscribeAll() 统一订阅
    }
    try {
        m_client->subscribe(topic, static_cast<int>(level));
        log_info("subscribed topic=" + topic);
    } catch (const std::exception& e) {
        log_info(std::string("subscribe failed: ") + e.what());
    }
}

void MyMqttClient::Unsubscribe(const std::string& topic) {
    {
        std::lock_guard<std::mutex> lk(m_subscriptionMutex);
        m_subscriptions.erase(topic);
    }

    std::lock_guard<std::mutex> lk(m_clientMutex);
    try {
        if (m_client && m_client->is_connected()) {
            m_client->unsubscribe(topic);
        }
    } catch (const std::exception& e) {
        log_info(std::string("unsubscribe failed: ") + e.what());
    }
}

void MyMqttClient::SetProtoHandler(MqttProtoHandler handler) {
    std::lock_guard<std::mutex> lk(m_subscriptionMutex);
    m_protoHandler = std::move(handler);
}

// ============================================================
// 自动重连后的重订阅
// ============================================================

void MyMqttClient::resubscribeAll() {
    std::map<std::string, int> topics;
    {
        std::lock_guard<std::mutex> lk(m_subscriptionMutex);
        topics = m_subscriptions;
    }
    if (topics.empty()) {
        return;
    }

    log_info("resubscribe after (re)connect, topics=" + std::to_string(topics.size()));
    std::lock_guard<std::mutex> lk(m_clientMutex);
    if (!m_client || !m_client->is_connected()) {
        return;
    }
    for (const auto& [topic, qos] : topics) {
        try {
            m_client->subscribe(topic, qos);
            log_info("resubscribed topic=" + topic);
        } catch (const std::exception& e) {
            log_info(std::string("resubscribe failed: ") + e.what());
        }
    }
}

// ============================================================
// 后台接收线程
// ============================================================

void MyMqttClient::StartReceiveLoop() {
    bool expected = false;
    if (!m_running.compare_exchange_strong(expected, true)) {
        return;  // 已经在跑
    }

    // consume_message() 依赖 Paho 的消息队列,必须先 start_consuming() 初始化,
    // 否则内部 queue 是空指针,收不到消息甚至可能崩溃。
    try {
        std::lock_guard<std::mutex> lk(m_clientMutex);
        if (m_client) {
            m_client->start_consuming();
        }
    } catch (const std::exception& e) {
        log_info(std::string("start_consuming failed: ") + e.what());
    }

    m_receiveThread = std::thread([this]() { receiveLoop(); });
}

void MyMqttClient::StopReceiveLoop() {
    bool expected = true;
    if (!m_running.compare_exchange_strong(expected, false)) {
        return;  // 没在跑
    }
    try {
        std::lock_guard<std::mutex> lk(m_clientMutex);
        if (m_client) {
            m_client->stop_consuming();
        }
    } catch (...) {
        // 忽略停止阶段的异常
    }
    if (m_receiveThread.joinable()) {
        m_receiveThread.join();
    }
}

void MyMqttClient::receiveLoop() {
    log_info("receive loop started, clientID=" + m_clientId);
    while (m_running.load()) {
        try {
            std::shared_ptr<mqtt::client> client;
            {
                std::lock_guard<std::mutex> lk(m_clientMutex);
                client = m_client;
            }
            if (!client || !client->is_connected()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            // 用带超时的 try_consume_message_for,让循环能响应退出;
            // 阻塞式 consume_message() 会卡住 StopReceiveLoop() 的 join。
            mqtt::const_message_ptr msg;
            if (!client->try_consume_message_for(&msg,
                                                 std::chrono::milliseconds(200))) {
                continue;  // 超时,无新消息
            }
            if (!msg) continue;

            const std::string& topic = msg->get_topic();
            const auto& pl = msg->get_payload();
            std::vector<uint8_t> payload(pl.begin(), pl.end());

            MqttProtoHandler handler;
            {
                std::lock_guard<std::mutex> lk(m_subscriptionMutex);
                handler = m_protoHandler;
            }
            if (handler) {
                handler(topic, payload);
            }
        } catch (const mqtt::exception& e) {
            log_info(std::string("recv exception: ") + e.what());
        } catch (const std::exception& e) {
            log_info(std::string("recv std exception: ") + e.what());
        }
    }
    log_info("receive loop stopped");
}
