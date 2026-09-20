#pragma once

// ============================================================
// bridge_mqtt_client.hpp -- MyMqttClient
// 自定义客户端的 MQTT 通信封装
//
// 数据格式: Protobuf v3 (>= 3.15) 序列化的二进制流
// 传输协议: MQTT (Paho MQTT C++ 客户端)
// 服务器  : 192.168.12.1:3333
// 客户端  : 192.168.12.2  (本机)
// clientID: 填入所连接的机器人 ID 编号
//
// 线程模型:
//   - 连接 / 断开 / 订阅 / 发布 由调用方线程发起, 内部用互斥锁串行化;
//   - 后台接收线程只做 consume 与回调分发, 不持有跨调用锁;
//   - 自动重连后由 mqtt::callback 触发重新订阅, 避免 clean_session=true
//     时断线重连导致订阅丢失的问题。
// ============================================================

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace mqtt { class client; class callback; }

enum class MqttQosLevel {
    AtMostOnce  = 0,
    AtLeastOnce = 1,
    ExactlyOnce = 2,
};

// 收到一条 Protobuf 二进制 payload 后的回调
using MqttProtoHandler = std::function<void(const std::string& topic,
                                             const std::vector<uint8_t>& payload)>;

class MyMqttClient {
public:
    // robot_id: 所连接的机器人 ID 编号,会作为 clientID 的一部分
    // ip/port : MQTT broker 地址
    MyMqttClient(uint32_t       robot_id,
                 const std::string& ip   = "192.168.12.1",
                 int                 port = 3333);
    ~MyMqttClient();

    MyMqttClient(const MyMqttClient&)            = delete;
    MyMqttClient& operator=(const MyMqttClient&) = delete;

    // 同步连接 broker。先尝试 MQTT 5.0, 失败后回退 3.1.1 (3.1)，
    // 避免 broker 只支持单一版本时握手失败。
    bool Connect();
    void Disconnect();
    bool IsConnected() const;

    // 通用发布接口:把已经序列化好的 Protobuf 二进制发布到 topic
    void PublishProto(const std::string& topic,
                      const std::vector<uint8_t>& proto_bytes,
                      MqttQosLevel level = MqttQosLevel::AtLeastOnce,
                      bool        retain = false);

    // 字符串便捷发布(若上层不方便序列化时使用)
    void PublishString(const std::string& topic,
                       const std::string& message,
                       MqttQosLevel level = MqttQosLevel::AtLeastOnce,
                       bool        retain = false);

    // 订阅一个 topic,收到消息后通过 handler 回调。
    // 订阅会被记录, 自动重连后自动重新订阅。
    void Subscribe(const std::string& topic,
                   MqttQosLevel level = MqttQosLevel::AtLeastOnce);
    void Unsubscribe(const std::string& topic);

    void SetProtoHandler(MqttProtoHandler handler);

    // 由 mqtt::callback 触发, 在自动重连后重新订阅所有已记录 topic。
    // 声明为 public 以便 ClientCallback 调用; 上层业务无需主动调用。
    void resubscribeAll();

    // 按指定 MQTT 版本重建底层 Paho 客户端 (先析构旧 client 再替换 callback,
    // 保证 callback 生命周期始终覆盖 client)。
    void recreateClient(int mqtt_version);

    // 启动/停止后台接收线程(consume_message 循环)
    void StartReceiveLoop();
    void StopReceiveLoop();

    uint32_t RobotId() const { return m_robotId; }
    const std::string& ClientId() const { return m_clientId; }

private:
    void receiveLoop();

    uint32_t    m_robotId;
    std::string m_ip;
    int         m_port;
    std::string m_clientId;   // 实际连接到 broker 的 clientID

    // 注意: m_callback 必须声明在 m_client 之前, 成员按声明逆序析构,
    // 保证 m_callback 比持有其引用的 m_client 更晚析构。
    std::shared_ptr<mqtt::callback> m_callback; // 连接状态回调 (重连后重订阅)
    std::shared_ptr<mqtt::client>   m_client;   // Paho 同步客户端

    mutable std::mutex m_clientMutex;       // 串行化 connect/disconnect/subscribe/publish
    std::mutex         m_subscriptionMutex; // 保护 m_subscriptions

    // topic -> QoS, 记录已订阅主题以便自动重连后重订阅
    std::map<std::string, int> m_subscriptions;

    MqttProtoHandler m_protoHandler;

    // 每个实例独立的接收线程状态 (不再使用全局静态变量, 支持多实例)
    std::atomic<bool> m_running{false};
    std::thread       m_receiveThread;
    std::atomic<bool> m_needsResubscribe{false};
};
