// ============================================================
// bridge_node.cpp -- 通信/图传端 ROS 2 节点 (BridgeNode)
//
// 职责:
//   - 启动并管理 bridge_client 子进程 (原 main.cpp 业务)
//   - 监听 ROS 2 shutdown 信号, 优雅结束子进程
//
// 设计:
//   bridge_client 是独立的可执行文件 (主业务仍在其中),
//   节点只负责"保姆"角色. 这样无需重写 MQTT / UDP / protobuf 业务代码.
// ============================================================

#include <rclcpp/rclcpp.hpp>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

class BridgeNode : public rclcpp::Node {
public:
    BridgeNode() : Node("bridge_node") {
        this->declare_parameter<std::string>("child_executable", "bridge_client");
        this->declare_parameter<int>("robot_id", 7);
        this->declare_parameter<std::string>("mqtt_broker_ip", "192.168.12.1");
        this->declare_parameter<int>("mqtt_broker_port", 3333);

        RCLCPP_INFO(this->get_logger(), "BridgeNode starting...");
        start_child();
    }

    ~BridgeNode() override {
        stop_child();
    }

private:
    void start_child() {
        std::string exe = this->get_parameter("child_executable").as_string();
        int rid         = this->get_parameter("robot_id").as_int();
        std::string ip  = this->get_parameter("mqtt_broker_ip").as_string();
        int port        = this->get_parameter("mqtt_broker_port").as_int();

        // 优先用 ./build/bridge/bridge_client (colcon 默认产物),
        // 找不到再用 PATH (安装后情况)
        std::vector<std::string> search_paths = {
            "./build/bridge/bridge_client",
            "bridge_client"
        };
        std::string resolved;
        for (const auto& p : search_paths) {
            if (access(p.c_str(), X_OK) == 0) { resolved = p; break; }
        }
        if (resolved.empty()) resolved = exe;

        pid_t pid = fork();
        if (pid < 0) {
            RCLCPP_ERROR(this->get_logger(), "fork failed: %s", std::strerror(errno));
            return;
        }
        if (pid == 0) {
            std::string rid_s   = std::to_string(rid);
            std::string port_s  = std::to_string(port);
            execlp(resolved.c_str(), resolved.c_str(),
                   "--robot-id", rid_s.c_str(),
                   "--broker-ip", ip.c_str(),
                   "--broker-port", port_s.c_str(),
                   (char*)nullptr);
            std::cerr << "[bridge_node] exec failed: " << std::strerror(errno) << std::endl;
            _exit(127);
        }

        child_pid_ = pid;
        RCLCPP_INFO(this->get_logger(),
                    "Started bridge_client (pid=%d, exe=%s)", pid, resolved.c_str());
        waiter_ = std::thread([this]() { wait_loop(); });
    }

    void wait_loop() {
        while (rclcpp::ok() && child_pid_ > 0) {
            int status = 0;
            pid_t r = waitpid(child_pid_, &status, WNOHANG);
            if (r == child_pid_) {
                if (WIFEXITED(status)) {
                    RCLCPP_WARN(this->get_logger(),
                                "bridge_client exited (code=%d)", WEXITSTATUS(status));
                } else if (WIFSIGNALED(status)) {
                    RCLCPP_WARN(this->get_logger(),
                                "bridge_client killed by signal %d", WTERMSIG(status));
                }
                child_pid_ = 0;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }

    void stop_child() {
        if (child_pid_ > 0) {
            RCLCPP_INFO(this->get_logger(),
                        "Stopping bridge_client (pid=%d)...", child_pid_);
            ::kill(child_pid_, SIGTERM);
            for (int i = 0; i < 50; ++i) {
                int status = 0;
                pid_t r = waitpid(child_pid_, &status, WNOHANG);
                if (r == child_pid_) { child_pid_ = 0; break; }
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            if (child_pid_ > 0) {
                ::kill(child_pid_, SIGKILL);
                waitpid(child_pid_, nullptr, 0);
                child_pid_ = 0;
            }
        }
        if (waiter_.joinable()) waiter_.join();
    }

    pid_t child_pid_ = 0;
    std::thread waiter_;
};

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<BridgeNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
