"""
bridge.launch.py -- 通信/图传链路端启动入口

启动:
  bridge_node (ROS 2 节点), 负责拉起并管理 bridge_client 子进程.
  业务参数 (robot_id, mqtt_broker_ip, mqtt_broker_port) 走 ROS 参数
  转发给 bridge_client.

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

    bridge_node = Node(
        package='bridge',
        executable='bridge_node',
        name='bridge_node',
        output='screen',
        emulate_tty=True,
        parameters=[{
            'child_executable': 'bridge_client',
            'robot_id': robot_id,
            'mqtt_broker_ip': broker_ip,
            'mqtt_broker_port': broker_port,
        }],
    )
    return LaunchDescription([
        DeclareLaunchArgument('robot_id', default_value='1'),
        DeclareLaunchArgument('mqtt_broker_ip', default_value='192.168.12.1'),
        DeclareLaunchArgument('mqtt_broker_port', default_value='3333'),
        bridge_node,
    ])
