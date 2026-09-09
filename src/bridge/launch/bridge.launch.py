"""
bridge.launch.py -- 通信/图传链路端启动入口

启动:
  bridge_node (ROS 2 节点), 负责拉起并管理 bridge_client 子进程.
  业务参数 (robot_id, mqtt_broker_ip, mqtt_broker_port) 走 ROS 参数
  转发给 bridge_client.
"""

from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    bridge_node = Node(
        package='bridge',
        executable='bridge_node',
        name='bridge_node',
        output='screen',
        emulate_tty=True,
        parameters=[{
            'child_executable': 'bridge_client',
            'robot_id':         7,                  # 7号英雄; 实际可由 launch 参数覆盖
            'mqtt_broker_ip':   '192.168.12.1',
            'mqtt_broker_port': 3333,
        }],
    )
    return LaunchDescription([bridge_node])
