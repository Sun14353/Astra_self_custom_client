# todo_login — 不同选手的登录选择界面（架构骨架）

Qt/QML 登录选择界面，用于在启动时选择不同的选手/操作手，选定后联动
`todo_control` 加载对应预设。当前仅为**架构骨架**，无实际功能实现。

## 目录结构

```
todo_login/
├── src/
│   ├── main.cpp                 # Qt 界面入口（骨架）
│   ├── player_profile_model.h   # 选手档案模型（QObject 骨架）
│   └── player_profile_model.cpp
├── qml/
│   ├── TodoLoginScreen.qml      # 登录选择界面（骨架）
│   └── qml.qrc                  # QML 资源
└── launch/
    └── todo_login.launch.py     # 启动骨架（ExecuteProcess）
```

## 与其他 todo_ 包的关系

```
todo_login (选择选手)  ──选定预设 id──▶ todo_presets (操作手习惯 YAML/JSON)
                                                    │
                                                    ▼
                                     todo_control (加载预设 + 应用映射 + 输出控制)
```

- 界面通过 `playerModel.availablePlayers()` 展示 `todo_presets` 中的选手预设。
- 选定后由 `todo_control` 加载对应映射（TODO 联动）。

## TODO 清单

- [ ] `PlayerProfileModel::availablePlayers`：扫描 `todo_presets` 预设列表。
- [ ] 界面列表改为动态绑定 `playerModel`（当前为占位 `ListModel`）。
- [ ] 确认登录后联动 `todo_control` 加载选定预设。
