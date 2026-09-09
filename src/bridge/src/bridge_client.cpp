// ============================================================
// main.cpp -- 自定义客户端示例
//
// 通信流程:
//   1) 用 protoc 生成的 C++ 类构造 Protobuf 消息
//   2) serializeToString / SerializeAsString 序列化为二进制
//   3) MyMqttClient::PublishProto() 把二进制发到 topic
//   4) 订阅方收到二进制后 ParseFromString / ParseFromArray 反序列化
//
// 此外: 通过 UDP 3334 端口接收 HEVC 图传码流
// ============================================================

#include "bridge_mqtt_client.hpp"
#include "udp_video_receiver.hpp"

// 自动生成: 在 build/proto_gen/ 目录下
#include "custom_client.pb.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

namespace {
std::atomic<bool> g_exit{false};

void on_signal(int) { g_exit.store(true); }

// 简易时间戳(毫秒)
uint64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(
               system_clock::now().time_since_epoch()).count();
}

void print_hex(const std::string& tag, const std::vector<uint8_t>& v) {
    std::cerr << tag << " (" << v.size() << " bytes): ";
    for (size_t i = 0; i < v.size() && i < 16; ++i) {
        char buf[4];
        std::snprintf(buf, sizeof(buf), "%02X ", v[i]);
        std::cerr << buf;
    }
    if (v.size() > 16) std::cerr << "...";
    std::cerr << std::endl;
}
} // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT,  on_signal);
    std::signal(SIGTERM, on_signal);

    // ------------------------------------------------------------
    // 解析 CLI 参数 (可由 bridge_node 透传, 也可手动运行)
    //   --robot-id   <id>     机器人 ID 编号
    //   --broker-ip  <ip>     MQTT broker IP
    //   --broker-port <port>  MQTT broker 端口
    // ------------------------------------------------------------
    uint32_t       robot_id = 7;
    std::string    broker_ip = "192.168.12.1";
    int            broker_port = 3333;
    for (int i = 1; i + 1 < argc; i += 2) {
        std::string k = argv[i];
        std::string v = argv[i + 1];
        if      (k == "--robot-id")    robot_id   = static_cast<uint32_t>(std::stoi(v));
        else if (k == "--broker-ip")   broker_ip  = v;
        else if (k == "--broker-port") broker_port = std::stoi(v);
    }

    // ------------------------------------------------------------
    // 1) 创建 MyMqttClient,内部 clientID 格式为 "robot_<id>"
    // ------------------------------------------------------------
    MyMqttClient mqtt(robot_id, broker_ip, broker_port);

    // 收到任意 topic 的 Protobuf 二进制后,统一反序列化入口
    mqtt.SetProtoHandler([](const std::string& topic,
                            const std::vector<uint8_t>& payload) {
        std::cerr << "\n[recv] topic=" << topic
                  << ", payload=" << payload.size() << " bytes" << std::endl;
        print_hex("  hex", payload);

        // 演示:如果 topic 是 status/robot,就反序列化为 RobotStatus
        if (topic == "status/robot") {
            custom_client::RobotStatus status;
            if (status.ParseFromArray(payload.data(),
                                      static_cast<int>(payload.size()))) {
                std::cerr << "  parsed RobotStatus:"
                          << " robot_id="  << status.header().robot_id()
                          << " seq="       << status.header().sequence()
                          << " mode="      << custom_client::RobotStatus_Mode_Name(status.mode())
                          << " bitrate="   << status.current_bitrate()
                          << " fps="       << status.fps()
                          << std::endl;
            } else {
                std::cerr << "  parse failed!" << std::endl;
            }
        }
    });

    if (!mqtt.Connect()) {
        std::cerr << "MQTT connect failed, exit." << std::endl;
        return 1;
    }
    mqtt.StartReceiveLoop();

    // 订阅机器人上报状态
    mqtt.Subscribe("status/robot");

    // ------------------------------------------------------------
    // 2) 构造一个 ControlCmd,序列化为二进制
    // ------------------------------------------------------------
    custom_client::ControlCmd cmd;
    cmd.mutable_header()->set_robot_id(robot_id);
    cmd.mutable_header()->set_timestamp_ms(now_ms());
    cmd.mutable_header()->set_sequence(1);
    cmd.set_cmd(custom_client::ControlCmd::START_VIDEO);
    cmd.set_target_bitrate_kbps(40);

    std::string cmd_bytes;
    if (!cmd.SerializeToString(&cmd_bytes)) {
        std::cerr << "ControlCmd serialize failed" << std::endl;
        return 1;
    }
    std::vector<uint8_t> cmd_vec(cmd_bytes.begin(), cmd_bytes.end());
    print_hex("ControlCmd bytes", cmd_vec);

    // 发布到 topic
    mqtt.PublishProto("cmd/control", cmd_vec);

    // ------------------------------------------------------------
    // 3) 演示 GenericCmd
    // ------------------------------------------------------------
    custom_client::GenericCmd gcmd;
    gcmd.mutable_header()->set_robot_id(robot_id);
    gcmd.mutable_header()->set_timestamp_ms(now_ms());
    gcmd.mutable_header()->set_sequence(2);
    gcmd.set_name("set_param");
    gcmd.add_args("exposure=10000");
    gcmd.add_args("gain=5");

    std::string gcmd_bytes;
    gcmd.SerializeToString(&gcmd_bytes);
    std::vector<uint8_t> gcmd_vec(gcmd_bytes.begin(), gcmd_bytes.end());
    mqtt.PublishProto("cmd/generic", gcmd_vec);

    // ------------------------------------------------------------
    // 4) UDP 3334 端口 HEVC 图传接收
    // ------------------------------------------------------------
    UdpVideoReceiver video(3334);
    video.SetFrameCallback([](const HevcFrame& f) {
        std::cerr << "[video] frame_id=" << f.frame_id
                  << ", hevc bytes=" << f.data.size() << std::endl;
        // 这里把 f.data 喂给 ffmpeg / gstreamer / 自研解码器
    });
    video.Start();

    // 主循环
    std::cerr << "\nCustom client running. Press Ctrl+C to exit.\n" << std::endl;
    while (!g_exit.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // 演示:每 2 秒发布一次心跳
        static auto last = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (now - last >= std::chrono::seconds(2)) {
            last = now;
            custom_client::ControlCmd ping;
            ping.mutable_header()->set_robot_id(robot_id);
            ping.mutable_header()->set_timestamp_ms(now_ms());
            ping.mutable_header()->set_sequence(
                static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()).count()));
            ping.set_cmd(custom_client::ControlCmd::PING);
            std::string pb;
            ping.SerializeToString(&pb);
            std::vector<uint8_t> v(pb.begin(), pb.end());
            mqtt.PublishProto("cmd/control", v);
        }
    }

    video.Stop();
    mqtt.Disconnect();
    std::cerr << "bye." << std::endl;
    return 0;
}
