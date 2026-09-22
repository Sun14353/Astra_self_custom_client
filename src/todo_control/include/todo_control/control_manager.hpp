// ============================================================
// todo_control / control_manager.hpp
// 架构骨架：自定义控制管理器。
//
// 职责（TODO 实现）：
//   1. 从 todo_presets 加载操作手习惯预设（YAML/JSON）
//   2. 应用/切换控制映射
//   3. 订阅输入，按映射输出机器人控制指令
// ============================================================
#pragma once

#include <string>

#include "todo_control/control_mapping.hpp"

namespace todo_control {

class ControlManager {
public:
  ControlManager();
  ~ControlManager();

  // TODO: 从预设文件（todo_presets 的 YAML/JSON）加载一套操作手习惯映射。
  bool loadPreset(const std::string& preset_file);

  // TODO: 应用指定映射（切换操作手习惯）。
  bool applyMapping(const ControlMapping& mapping);

  // TODO: 输入事件入口（摇杆/按键/鼠标等）。
  void onInput(const std::string& source, double value);

  // TODO: 周期执行，输出控制指令到机器人。
  void tick();

private:
  ControlMapping m_current;  // TODO: 当前生效的映射

  // TODO: 输入设备抽象、控制指令发布器等。
};

} // namespace todo_control
