"""
ui_decoder.launch.py —— 启动 ui_decoder 节点 (Qt5 H.264 解码 + 瞄准 UI)。

从 EGA_Client/RM26_Client 迁移的 UI 界面：
  - VideoBackgroundWidget : 视频帧显示 (保持宽高比居中) + FPS / SIGNAL LOST 叠加
  - CrosshairWidget       : 准星 (Sniper / Default / Shotgun)
  - HeatRingWidget        : 热量环 (过热警告动画)
  - MatchHud.qml          : QML 赛场信息 HUD (比分/倒计时/基地/机器人状态)

节点订阅 doorlock_sniper 发布的 VideoPacket 码流 (默认 /video_stream)，
并通过 MQTT 订阅协议中自定义客户端下行 topic (GameStatus/...)。

robot_id / mqtt_broker_ip / mqtt_broker_port 支持从上层 launch 传入，
便于 all_bringup 统一编排。
"""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    robot_id = LaunchConfiguration('robot_id')
    broker_ip = LaunchConfiguration('mqtt_broker_ip')
    broker_port = LaunchConfiguration('mqtt_broker_port')

    decoder_node = Node(
        package='ui_decoder',
        executable='ui_decoder_node',
        name='ui_decoder',
        output='screen',
        emulate_tty=True,
        parameters=[{
            'topic': '/video_stream',
            'crosshair_type': 'sniper',
            'crosshair_size': 40,
            'crosshair_thickness': 2,
            'crosshair_color': '#00ff00',
            'glow_enabled': True,
            'pulse_enabled': False,
            'hitmarker_enabled': True,
            'dynamic_size': False,
            'heat_max': 240,
            'heat_current': 60,
            'demo_heat': True,
            'heat_glow': True,
            'heat_particles': True,
            'rotate_180': False,
            'window_width': 800,
            'window_height': 800,
            # 赛场信息下行 MQTT（协议中自定义客户端 topic）
            'enable_match_client': True,
            'mqtt_broker_ip': broker_ip,
            'mqtt_broker_port': broker_port,
            'robot_id': robot_id,
        }],
    )

    return LaunchDescription([
        DeclareLaunchArgument('robot_id', default_value='7'),
        DeclareLaunchArgument('mqtt_broker_ip', default_value='192.168.12.1'),
        DeclareLaunchArgument('mqtt_broker_port', default_value='3333'),
        decoder_node,
    ])
