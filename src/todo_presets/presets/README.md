# todo_presets — 操作手习惯预设（架构骨架）

不同选手/操作手习惯的 **YAML/JSON** 预设文件。每个文件描述一位操作手
（或一种操作风格）的输入习惯与自定义控制偏好，由 `todo_login` 展示、
`todo_control` 加载。

> 说明：本目录当前仅为**架构骨架**，示例预设中的数值均为占位，无实际功能。

## 目录结构

```
presets/
├── schema/
│   └── todo_preset.schema.json   # 预设格式 JSON Schema（骨架）
└── players/
    ├── todo_default.yaml         # 默认（均衡型）
    ├── todo_sniper.yaml          # 狙击型
    ├── todo_aggressive.yaml      # 进攻型
    ├── todo_support.yaml         # 支援型
    └── todo_sniper.json          # JSON 格式示例（与 YAML 等价）
```

## 命名约定

- 文件前缀统一 `todo_`，例如 `todo_sniper.yaml`。
- 同一预设可同时提供 `.yaml` 与 `.json` 两种格式（字段一致）。
- `id` 字段建议与文件名一致，便于 `todo_login` / `todo_control` 关联。

## 字段说明（骨架）

| 字段 | 类型 | 说明 |
|------|------|------|
| `id` | string | 预设唯一标识 |
| `name` | string | 展示名称（选手/风格名） |
| `description` | string | 描述 |
| `control.mouse` | object | 鼠标习惯（灵敏度/反向） |
| `control.joystick` | object | 摇杆习惯（死区/响应曲线） |
| `control.keyboard` | object | 键位习惯（TODO） |
| `custom_control.mapping` | array | 自定义控制映射项（对应 todo_control） |

## TODO 清单

- [ ] 明确与 `todo_control/control_mapping.hpp` 的字段映射。
- [ ] 补充真实选手预设与键位映射。
- [ ] 在 `todo_login` / `todo_control` 中实现加载逻辑。
