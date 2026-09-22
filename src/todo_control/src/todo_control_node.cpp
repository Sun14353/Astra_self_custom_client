// ============================================================
// todo_control_node.cpp
// 架构骨架：自定义控制 ROS 2 节点入口。
//
// 当前仅完成节点生命周期骨架；实际控制逻辑 TODO：
//   - 实例化 ControlManager
//   - 加载操作手预设（来自 todo_presets，由 todo_login 选择）
//   - 订阅输入话题，发布自定义控制指令
// ============================================================
#include <rclcpp/rclcpp.hpp>

#include "todo_control/control_manager.hpp"

int main(int argc, char** argv) {
  rclcpp::init(argc, argv);

  auto node = std::make_shared<rclcpp::Node>("todo_control_node");

  // TODO: 在此接入 ControlManager 与输入/输出话题。
  // todo_control::ControlManager manager;

  RCLCPP_INFO(node->get_logger(),
              "todo_control_node skeleton ready (TODO: implement control logic)");

  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
