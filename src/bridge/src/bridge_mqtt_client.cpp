// ============================================================
// msg.cpp -- MyMqttClient 实现
//
// 通信流程:
//   1) 自定义客户端将 Protobuf 对象序列化为二进制
//   2) 调用 publish() 把二进制发到对应的 topic
//   3) 订阅方收到二进制后,用 Protobuf 反序列化
// ============================================================

#include "bridge_mqtt_client.hpp"

#include <mqtt/client.h>

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
std::mutex        g_logMutex;

void log_info(const std::string& msg) {
    std::lock_guard<std::mutex> lk(g_logMutex);
    std::cerr << "[MyMqttClient] " << msg << std::endl;
}
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
    // 格式: "robot_<id>"  (例如 robot_7 表示 7 号英雄)
    // ------------------------------------------------------------
    m_clientId = "robot_" + std::to_string(m_robotId);

    const std::string serverURI = "tcp://" + m_ip + ":" + std::to_string(m_port);
    m_client = std::make_shared<mqtt::client>(serverURI, m_clientId);
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
}

// ============================================================
// 连接 / 断开
// ============================================================

bool MyMqttClient::Connect() {
    try {
        mqtt::connect_options opts;
        opts.set_keep_alive_interval(20);
        opts.set_clean_session(true);
        opts.set_automatic_reconnect(true);
        opts.set_connect_timeout(std::chrono::seconds(5));

        // Paho MQTT C++ 1.2.0: connect() 是同步调用,返回 connect_response
        // 超时由 connect_options 内部处理
        m_client->connect(opts);
        return m_client->is_connected();
    } catch (const std::exception& e) {
        log_info(std::string("connect failed: ") + e.what());
        return false;
    }
}

void MyMqttClient::Disconnect() {
    StopReceiveLoop();
    try {
        if (m_client && m_client->is_connected()) {
            m_client->disconnect();
        }
    } catch (const std::exception& e) {
        log_info(std::string("disconnect failed: ") + e.what());
    }
}

bool MyMqttClient::IsConnected() const {
    return m_client && m_client->is_connected();
}

// ============================================================
// 发布:Protobuf 二进制 / 字符串
// ============================================================

void MyMqttClient::PublishProto(const std::string& topic,
                                const std::vector<uint8_t>& proto_bytes,
                                MqttQosLevel level,
                                bool retain) {
    if (!IsConnected()) {
        log_info("PublishProto: not connected, drop message topic=" + topic);
        return;
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
    if (!IsConnected()) {
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
    try {
        m_client->subscribe(topic, static_cast<int>(level));
        log_info("subscribed topic=" + topic);
    } catch (const std::exception& e) {
        log_info(std::string("subscribe failed: ") + e.what());
    }
}

void MyMqttClient::Unsubscribe(const std::string& topic) {
    try {
        m_client->unsubscribe(topic);
    } catch (const std::exception& e) {
        log_info(std::string("unsubscribe failed: ") + e.what());
    }
}

void MyMqttClient::SetProtoHandler(MqttProtoHandler handler) {
    m_protoHandler = std::move(handler);
}

// ============================================================
// 后台接收线程
// ============================================================

namespace {
std::atomic<bool> g_running{false};
std::thread       g_loopThread;
}

void MyMqttClient::StartReceiveLoop() {
    bool expected = false;
    if (!g_running.compare_exchange_strong(expected, true)) {
        return;  // 已经在跑
    }
    g_loopThread = std::thread([this]() {
        log_info("receive loop started, clientID=" + m_clientId);
        while (g_running.load()) {
            try {
                if (!m_client || !m_client->is_connected()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }
                auto msg = m_client->consume_message();
                if (!msg) continue;

                const std::string& topic = msg->get_topic();
                const auto& pl = msg->get_payload();
                std::vector<uint8_t> payload(pl.begin(), pl.end());

                if (m_protoHandler) {
                    m_protoHandler(topic, payload);
                }
            } catch (const mqtt::exception& e) {
                log_info(std::string("recv exception: ") + e.what());
            } catch (const std::exception& e) {
                log_info(std::string("recv std exception: ") + e.what());
            }
        }
        log_info("receive loop stopped");
    });
}

void MyMqttClient::StopReceiveLoop() {
    bool expected = true;
    if (!g_running.compare_exchange_strong(expected, false)) {
        return;  // 没在跑
    }
    if (g_loopThread.joinable()) {
        g_loopThread.join();
    }
}
