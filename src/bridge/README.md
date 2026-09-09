# bridge — 通信/图传链路端

本包封装的是**通信 + 图传**链路的客户端侧，独立于相机/编码/解码这些"发射端"算法包。

## 包内可执行文件

| 名字            | 类型       | 作用 |
|-----------------|-----------|------|
| `bridge_client` | 纯 C++    | 主业务进程：连接 MQTT broker、订阅/发布 Protobuf 消息、UDP 3334 接收 HEVC 图传码流。**不**依赖 ROS 2，可以单独跑 |
| `bridge_node`   | ROS 2 节点 | 启动并管理 `bridge_client` 子进程的生命周期（Ctrl+C 优雅退出、`rclcpp::spin` 等） |

## Launch 文件

| 文件                    | 启动内容 |
|-------------------------|----------|
| `launch/bridge.launch.py`           | 仅 `bridge_node`（通信端） |
| `launch/all_bringup.launch.py`      | **项目级总入口** —— `bridge.launch.py` + `bringup/launch/doorlock.launch.py` |

## 编译

```bash
# 在工作区根目录
colcon build --packages-select bridge
# 或一次性全 build
colcon build
```

## 运行

```bash
# 推荐: 用总 launch, 通信端 + 发射端 一起拉起
ros2 launch bridge all_bringup.launch.py

# 单独跑通信端
ros2 launch bridge bridge.launch.py

# 直接跑底层业务进程 (不用 ROS 2 也能用)
./build/bridge/bridge_client --robot-id 7 --broker-ip 192.168.12.1 --broker-port 3333
```
