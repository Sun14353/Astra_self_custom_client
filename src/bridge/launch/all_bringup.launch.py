"""
all_bringup.launch.py -- 项目级总启动入口

同时启动:
  1) 通信/图传链路端 (bridge.launch.py)              <-- 通信端
  2) 发射端 (bringup/launch/doorlock.launch.py)      <-- ROS 2 相机+编码+解码
  3) UI 解码端 (ui_decoder/launch/ui_decoder.launch.py) <-- Qt5 瞄准 UI + QML HUD
  4) 赛场信息模拟器 (bridge/launch/match_simulator.launch.py) <-- MQTT 赛场信息源

robot_id / mqtt_broker_ip / mqtt_broker_port 在顶层统一声明并下发给
bridge / ui_decoder / match_simulator，保证三者连到同一 broker、
使用同一机器人编号。

注意: 第 4 项 match_simulator 是脱离赛事引擎的联调模拟源；
      真机部署时若裁判系统/机器人已发布同样的 topic，应删除该 Include。
"""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from ament_index_python.packages import get_package_share_directory
import os


def generate_launch_description():
    robot_id = LaunchConfiguration('robot_id')
    broker_ip = LaunchConfiguration('mqtt_broker_ip')
    broker_port = LaunchConfiguration('mqtt_broker_port')

    # 1) 通信端: include bridge 自身的 bridge.launch.py
    bridge_share = get_package_share_directory('bridge')
    bridge_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(bridge_share, 'launch', 'bridge.launch.py')
        ),
        launch_arguments={
            'robot_id': robot_id,
            'mqtt_broker_ip': broker_ip,
            'mqtt_broker_port': broker_port,
        }.items(),
    )

    # 2) 发射端: include bringup 包里的 doorlock.launch.py
    bringup_share = get_package_share_directory('bringup')
    doorlock_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(bringup_share, 'launch', 'doorlock.launch.py')
        )
    )

    # 3) UI 解码端: include ui_decoder 包里的 ui_decoder.launch.py
    ui_decoder_share = get_package_share_directory('ui_decoder')
    ui_decoder_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(ui_decoder_share, 'launch', 'ui_decoder.launch.py')
        ),
        launch_arguments={
            'robot_id': robot_id,
            'mqtt_broker_ip': broker_ip,
            'mqtt_broker_port': broker_port,
        }.items(),
    )

    # 4) 赛场信息模拟器: include bridge 包里的 match_simulator.launch.py
    match_simulator_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(bridge_share, 'launch', 'match_simulator.launch.py')
        ),
        launch_arguments={
            'robot_id': robot_id,
            'mqtt_broker_ip': broker_ip,
            'mqtt_broker_port': broker_port,
        }.items(),
    )

    return LaunchDescription([
        DeclareLaunchArgument('robot_id', default_value='1'),
        DeclareLaunchArgument('mqtt_broker_ip', default_value='192.168.12.1'),
        DeclareLaunchArgument('mqtt_broker_port', default_value='3333'),
        bridge_launch,
        doorlock_launch,
        ui_decoder_launch,
        match_simulator_launch,
    ])
