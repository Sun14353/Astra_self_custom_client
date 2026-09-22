"""
todo_control.launch.py -- 自定义控制节点启动骨架

TODO: 补充 preset_file 等参数与依赖节点（输入设备、机器人控制等），
      并从 todo_login 选择的选手预设注入参数。
"""

from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        Node(
            package='todo_control',
            executable='todo_control_node',
            name='todo_control_node',
            output='screen',
            # TODO: 从 todo_login 选择的选手预设注入 preset_file 参数
            parameters=[{'preset_file': ''}],
        ),
    ])
