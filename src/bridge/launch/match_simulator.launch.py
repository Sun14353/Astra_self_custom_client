"""
match_simulator.launch.py -- 赛场信息发布模拟器启动入口

通过 ExecuteProcess 拉起 bridge 包里的 match_simulator 可执行文件
(纯 C++，不依赖 ROS 2)，向 MQTT broker 发布赛场信息下行 topic
(GameStatus / RobotDynamicStatus / RobotPosition / GlobalUnitStatus / ...)，
供 ui_decoder 的 QML HUD 订阅并实时显示。
"""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess
from launch.substitutions import LaunchConfiguration
from ament_index_python.packages import get_package_prefix
import os


def generate_launch_description():
    robot_id = LaunchConfiguration('robot_id')
    broker_ip = LaunchConfiguration('mqtt_broker_ip')
    broker_port = LaunchConfiguration('mqtt_broker_port')

    exe = os.path.join(
        get_package_prefix('bridge'), 'lib', 'bridge', 'match_simulator')

    return LaunchDescription([
        DeclareLaunchArgument('robot_id', default_value='7'),
        DeclareLaunchArgument('mqtt_broker_ip', default_value='192.168.12.1'),
        DeclareLaunchArgument('mqtt_broker_port', default_value='3333'),

        ExecuteProcess(
            cmd=[
                exe,
                '--robot-id', robot_id,
                '--broker-ip', broker_ip,
                '--broker-port', broker_port,
            ],
            name='match_simulator',
            output='screen',
            emulate_tty=True,
        ),
    ])
