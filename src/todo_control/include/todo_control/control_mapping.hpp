// ============================================================
// todo_control / control_mapping.hpp
// 架构骨架：自定义控制映射的数据结构定义。
//
// 说明：仅定义数据载体，无实际逻辑。具体映射规则在 TODO 阶段实现，
//       数据结构需与 todo_presets 中的 YAML/JSON 预设字段对齐。
// ============================================================
#pragma once

#include <string>
#include <vector>

namespace todo_control {

// 单个输入源 -> 控制动作 的映射项（骨架）。
struct ControlMappingEntry {
  std::string source;      // TODO: 输入源标识（轴 / 按键 / 摇杆通道）
  std::string action;      // TODO: 目标控制动作标识
  double scale = 1.0;      // TODO: 灵敏度 / 缩放系数
  bool invert = false;     // TODO: 是否反向
};

// 一整套操作手习惯的自定义映射（骨架）。
struct ControlMapping {
  std::string name;                              // 映射名（对应 todo_presets 预设 id）
  std::vector<ControlMappingEntry> entries;      // 映射项列表
};

} // namespace todo_control
