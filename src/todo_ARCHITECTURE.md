# todo_ 系列 — 自定义控制 / 预设 / 登录选择（架构总览）

三个新增的 `todo_` 前缀工作空间，共同构成「选手登录 → 选择预设 → 自定义控制」链路。
当前**仅完成架构骨架**，无实际功能实现；所有待实现点均以 `TODO` 标记。

## 包清单

| 包 | 类型 | 职责 |
|----|------|------|
| [todo_login](todo_login/) | Qt/QML 界面 | 不同选手的登录选择界面 |
| [todo_presets](todo_presets/) | 纯配置 | 不同操作手习惯的 YAML/JSON 预设文件 |
| [todo_control](todo_control/) | C++ 节点 | 自定义控制（加载预设 + 应用映射 + 输出控制指令） |

## 数据流

```
todo_login (选择选手)
     │  选定预设 id
     ▼
todo_presets (操作手习惯 YAML/JSON)
     │  预设被加载
     ▼
todo_control (应用映射 + 输出控制)
```

## 目录结构

```
src/
├── todo_ARCHITECTURE.md      # 本文件（总览）
├── todo_login/               # 登录选择界面（Qt/QML）
├── todo_presets/             # 操作手习惯预设（YAML/JSON）
└── todo_control/             # 自定义控制节点（C++）
```

## 后续集成点（TODO）

- [ ] 在 `bridge/launch/all_bringup.launch.py` 聚合启动 todo_login / todo_control。
- [ ] 明确 `todo_presets` 预设字段与 `todo_control/control_mapping.hpp` 的映射。
- [ ] 打通 todo_login 选择结果 → todo_control 的参数注入链路。
