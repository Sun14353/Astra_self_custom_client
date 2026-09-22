// ============================================================
// match_simulator.cpp -- 赛场信息发布模拟器
//
// 模仿 EGA_Client 的 sim 端 (MQTTPublisher + MatchSimulationEngine),
// 提供一个脱离赛事引擎也能运行的 MQTT 赛场信息源:
//   - GameStatus          (5 Hz)  比赛阶段 / 比分 / 倒计时
//   - RobotPosition       (10 Hz) 8 台机器人位置
//   - RobotDynamicStatus  (10 Hz) 当前机器人血量 / 热量 / 弹药
//   - GlobalUnitStatus    (1 Hz)  基地 / 前哨站 / 全体机器人血量
//   - RobotStaticStatus   (1 Hz)  当前机器人静态属性
//   - Event / PenaltyInfo (触发式) 比赛事件与判罚
//
// 数据通过 Protobuf 序列化后发布到 MQTT, 供 bridge_client / ui_decoder
// 等下行端订阅。字段编号与 RoboMaster 2026 官方协议一致。
// ============================================================

#include "bridge_mqtt_client.hpp"

// 自动生成: 在 build/bridge/proto_gen/ 目录下
#include "custom_client.pb.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {
std::atomic<bool> g_exit{false};

void on_signal(int) { g_exit.store(true); }

struct RobotPose {
    float x = 0.0f;
    float y = 0.0f;
    float yaw = 0.0f;
};

// 与 EGA 模拟器一致的 8 台机器人: 红方 1/2/3/4, 蓝方 101/102/103/104
const std::vector<uint32_t> kRobotIds = {1, 2, 3, 4, 101, 102, 103, 104};

std::vector<RobotPose> buildPositions(double elapsed_sec) {
    std::vector<RobotPose> poses;
    for (size_t i = 0; i < kRobotIds.size(); ++i) {
        const bool red = i < 4;
        const double phase = static_cast<double>(i) * 0.7;
        const double wave = std::sin(elapsed_sec * 0.35 + phase);
        const double forward = 3.5 + (i % 4) * 1.2 + 2.0 * (0.5 + 0.5 * wave);
        const double x = red ? forward : 28.0 - forward;
        const double y = 2.2 + (i % 4) * 3.25 + 1.4 * std::sin(elapsed_sec * 0.27 + phase);
        const double yaw = std::atan2(std::cos(elapsed_sec * 0.3 + phase),
                                      red ? 1.0 : -1.0);
        poses.push_back({static_cast<float>(x),
                         static_cast<float>(y),
                         static_cast<float>(yaw * 180.0 / 3.14159265358979)});
    }
    return poses;
}

int buildHealth(double elapsed_sec, size_t index) {
    const double wave = 0.5 + 0.5 * std::sin(elapsed_sec * (0.5 + (index % 3) * 0.2) + index);
    return std::max(140, 600 - static_cast<int>((250 + 20 * (index % 4)) * wave));
}

int stageFromElapsed(double elapsed_sec) {
    // 简化阶段推进: 5s 准备倒计时 -> 战斗 -> 结算
    if (elapsed_sec < 5.0) return 3;          // 准备阶段
    if (elapsed_sec < 425.0) return 4;        // 战斗阶段
    return 5;                                  // 结算阶段
}

void publishProto(MyMqttClient& mqtt, const std::string& topic,
                  const google::protobuf::MessageLite& msg) {
    std::string bytes;
    if (!msg.SerializeToString(&bytes)) {
        std::cerr << "[match_simulator] serialize failed: " << topic << std::endl;
        return;
    }
    std::vector<uint8_t> vec(bytes.begin(), bytes.end());
    mqtt.PublishProto(topic, vec);
}

void publishGameStatus(MyMqttClient& mqtt, double elapsed_sec) {
    custom_client::GameStatus msg;
    msg.set_current_round(1);
    msg.set_total_rounds(2);
    msg.set_red_score(static_cast<uint32_t>(elapsed_sec * 0.05));
    msg.set_blue_score(static_cast<uint32_t>(elapsed_sec * 0.04));

    const int stage = stageFromElapsed(elapsed_sec);
    msg.set_current_stage(stage);
    if (stage == 3) {
        msg.set_stage_countdown_sec(std::max(1, static_cast<int>(std::ceil(5.0 - elapsed_sec))));
        msg.set_stage_elapsed_sec(0);
    } else if (stage == 4) {
        const double battle = elapsed_sec - 5.0;
        msg.set_stage_countdown_sec(std::max(0, 420 - static_cast<int>(battle)));
        msg.set_stage_elapsed_sec(static_cast<int>(battle));
    } else {
        msg.set_stage_countdown_sec(0);
        msg.set_stage_elapsed_sec(420);
        msg.set_game_result(1);
        msg.set_end_reason(1);
    }
    msg.set_is_paused(false);
    publishProto(mqtt, "GameStatus", msg);
}

void publishRobotPositions(MyMqttClient& mqtt, const std::vector<RobotPose>& poses) {
    for (size_t i = 0; i < poses.size(); ++i) {
        custom_client::RobotPosition msg;
        msg.set_x(poses[i].x);
        msg.set_y(poses[i].y);
        msg.set_z(0.0f);
        msg.set_yaw(poses[i].yaw);
        msg.set_robot_id(kRobotIds[i]);
        publishProto(mqtt, "RobotPosition", msg);
    }
}

void publishRobotDynamicStatus(MyMqttClient& mqtt,
                               double elapsed_sec, size_t index) {
    custom_client::RobotDynamicStatus msg;
    msg.set_current_health(static_cast<uint32_t>(buildHealth(elapsed_sec, index)));
    msg.set_current_heat(static_cast<float>(std::fmod(elapsed_sec * 40.0 + index * 15.0, 240.0)));
    msg.set_last_projectile_fire_rate(8.0f);
    msg.set_current_chassis_energy(100);
    msg.set_current_buffer_energy(40);
    msg.set_current_experience(static_cast<uint32_t>(elapsed_sec * 10));
    msg.set_experience_for_upgrade(1000);
    msg.set_total_projectiles_fired(static_cast<uint32_t>(elapsed_sec * 8));
    msg.set_remaining_ammo(500 - static_cast<uint32_t>(elapsed_sec * 8) % 500);
    msg.set_is_out_of_combat(false);
    msg.set_out_of_combat_countdown(0);
    msg.set_can_remote_heal(true);
    msg.set_can_remote_ammo(true);
    publishProto(mqtt, "RobotDynamicStatus", msg);
}

void publishGlobalUnitStatus(MyMqttClient& mqtt, double elapsed_sec) {
    custom_client::GlobalUnitStatus msg;
    msg.set_base_health(5000 - static_cast<uint32_t>(elapsed_sec * 2));
    msg.set_base_status(0);
    msg.set_base_shield(0);
    msg.set_outpost_health(std::max(0, 1500 - static_cast<int>(elapsed_sec * 2.5)));
    msg.set_outpost_status(1);
    msg.set_enemy_base_health(5000);
    msg.set_enemy_base_status(0);
    msg.set_enemy_base_shield(0);
    msg.set_enemy_outpost_health(1500);
    msg.set_enemy_outpost_status(1);

    for (size_t i = 0; i < kRobotIds.size(); ++i) {
        msg.add_robot_health(static_cast<uint32_t>(buildHealth(elapsed_sec, i)));
        msg.add_robot_bullets(500);
    }
    msg.set_total_damage_ally(static_cast<uint32_t>(elapsed_sec * 30));
    msg.set_total_damage_enemy(static_cast<uint32_t>(elapsed_sec * 25));
    publishProto(mqtt, "GlobalUnitStatus", msg);
}

void publishRobotStaticStatus(MyMqttClient& mqtt, uint32_t robot_id) {
    custom_client::RobotStaticStatus msg;
    msg.set_connection_state(1);
    msg.set_field_state(1);
    msg.set_alive_state(1);
    msg.set_robot_id(robot_id);
    msg.set_robot_type(robot_id >= 100 ? robot_id - 100 : robot_id);
    msg.set_performance_system_shooter(1);
    msg.set_performance_system_chassis(1);
    msg.set_level(1);
    msg.set_max_health(600);
    msg.set_max_heat(240);
    msg.set_heat_cooldown_rate(20.0f);
    msg.set_max_power(120);
    msg.set_max_buffer_energy(60);
    msg.set_max_chassis_energy(120);
    publishProto(mqtt, "RobotStaticStatus", msg);
}

void publishTimedEvents(MyMqttClient& mqtt, double elapsed_sec, int* last_event) {
    struct TimedEvent {
        double at;
        int id;
        std::string param;
    };
    static const std::vector<TimedEvent> kEvents = {
        {10.0, 4, "1"},     // 己方激活小能量机关
        {30.0, 5, "100"},   // 己方英雄完成狙击
        {60.0, 1, ""},      // 己方击毁对方步兵
        {90.0, 9, ""},      // 己方飞镖命中对方前哨站
        {120.0, 2, ""},     // 对方前哨站被摧毁
        {150.0, 11, ""},    // 己方基地遭到攻击
    };
    for (size_t i = 0; i < kEvents.size(); ++i) {
        if (*last_event < static_cast<int>(i + 1) && elapsed_sec >= kEvents[i].at) {
            custom_client::Event msg;
            msg.set_event_id(kEvents[i].id);
            msg.set_param(kEvents[i].param);
            publishProto(mqtt, "Event", msg);
            *last_event = static_cast<int>(i + 1);
            std::cerr << "[match_simulator] Event id=" << kEvents[i].id << std::endl;
        }
    }
}
} // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    uint32_t robot_id = 7;
    std::string broker_ip = "192.168.12.1";
    int broker_port = 3333;
    for (int i = 1; i + 1 < argc; i += 2) {
        std::string k = argv[i];
        std::string v = argv[i + 1];
        if (k == "--robot-id") robot_id = static_cast<uint32_t>(std::stoi(v));
        else if (k == "--broker-ip") broker_ip = v;
        else if (k == "--broker-port") broker_port = std::stoi(v);
    }

    MyMqttClient mqtt(robot_id, broker_ip, broker_port);
    if (!mqtt.Connect()) {
        std::cerr << "[match_simulator] MQTT connect failed, exit." << std::endl;
        return 1;
    }
    mqtt.StartReceiveLoop();

    const auto start = std::chrono::steady_clock::now();
    double last1hz = 0.0, last5hz = 0.0, last10hz = 0.0;
    int last_event = 0;

    std::cerr << "[match_simulator] publishing match info (robot_id=" << robot_id
              << ", broker=" << broker_ip << ":" << broker_port << ")" << std::endl;

    while (!g_exit.load()) {
        const double elapsed =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

        if (elapsed - last10hz >= 0.1) {
            last10hz = elapsed;
            auto poses = buildPositions(elapsed);
            publishRobotPositions(mqtt, poses);
            // 只发布当前视角机器人的动态状态
            size_t idx = 0;
            for (size_t i = 0; i < kRobotIds.size(); ++i) {
                if (kRobotIds[i] == robot_id) idx = i;
            }
            publishRobotDynamicStatus(mqtt, elapsed, idx);
        }

        if (elapsed - last5hz >= 0.2) {
            last5hz = elapsed;
            publishGameStatus(mqtt, elapsed);
            publishTimedEvents(mqtt, elapsed, &last_event);
        }

        if (elapsed - last1hz >= 1.0) {
            last1hz = elapsed;
            publishGlobalUnitStatus(mqtt, elapsed);
            publishRobotStaticStatus(mqtt, robot_id);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    mqtt.Disconnect();
    std::cerr << "[match_simulator] bye." << std::endl;
    return 0;
}
