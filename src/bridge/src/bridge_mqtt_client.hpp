#pragma once

// ============================================================
// msg.hpp -- MyMqttClient
// 自定义客户端的 MQTT 通信封装
//
// 数据格式: Protobuf v3 (>= 3.15) 序列化的二进制流
// 传输协议: MQTT (Paho MQTT C++ 客户端)
// 服务器  : 192.168.12.1:3333
// 客户端  : 192.168.12.2  (本机)
// clientID: 填入所连接的机器人 ID 编号
// ============================================================

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace mqtt { class client; }

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

    // 同步连接 broker
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

    // 订阅一个 topic,收到消息后通过 handler 回调
    void Subscribe(const std::string& topic,
                   MqttQosLevel level = MqttQosLevel::AtLeastOnce);
    void Unsubscribe(const std::string& topic);

    void SetProtoHandler(MqttProtoHandler handler);

    // 启动/停止后台接收线程(consume_message 循环)
    void StartReceiveLoop();
    void StopReceiveLoop();

    uint32_t RobotId() const { return m_robotId; }
    const std::string& ClientId() const { return m_clientId; }

private:
    uint32_t    m_robotId;
    std::string m_ip;
    int         m_port;
    std::string m_clientId;   // 实际连接到 broker 的 clientID

    std::shared_ptr<mqtt::client> m_client;

    MqttProtoHandler m_protoHandler;
};
