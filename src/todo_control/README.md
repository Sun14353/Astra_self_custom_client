# todo_control — 自定义控制工作空间（架构骨架）

自定义控制层。负责读取操作手预设、应用控制映射、将输入转换为机器人控制指令。

> 说明：本包当前仅为**架构骨架**，无实际功能实现；所有逻辑点均以 `TODO` 标记。

## 目录结构

```
todo_control/
├── include/todo_control/
│   ├── control_mapping.hpp   # 控制映射数据结构（骨架）
│   └── control_manager.hpp   # 控制管理器接口（骨架）
├── src/
│   ├── control_manager.cpp   # 控制管理器桩实现
│   └── todo_control_node.cpp # ROS 2 节点入口（骨架）
├── config/
│   └── todo_control_params.yaml  # 节点参数骨架
└── launch/
    └── todo_control.launch.py    # 启动骨架
```

## 与其他 todo_ 包的关系

```
todo_login  (选择选手)  ──选定预设 id──▶ todo_presets (操作手习惯 YAML/JSON)
                                                    │
                                                    ▼
                                     todo_control (加载预设 + 应用映射 + 输出控制)
```

- `todo_login`：选手登录选择界面，选定某个操作手/选手。
- `todo_presets`：不同操作手习惯的 YAML/JSON 预设文件。
- `todo_control`：读取上述预设并执行自定义控制（本包）。

## TODO 清单

- [ ] `ControlManager::loadPreset`：解析 YAML/JSON 预设。
- [ ] `ControlManager::applyMapping`：切换操作手习惯。
- [ ] `ControlManager::onInput` / `tick`：输入事件与周期输出。
- [ ] `todo_control_node`：接入输入/输出话题与参数。
