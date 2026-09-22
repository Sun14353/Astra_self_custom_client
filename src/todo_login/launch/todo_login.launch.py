"""
todo_login.launch.py -- 选手登录选择界面启动骨架

通过 ExecuteProcess 拉起 todo_login 的 Qt 界面程序（纯 Qt，非 ROS 节点）。
TODO: 与 todo_control / todo_presets 的联动参数注入。
"""

from launch import LaunchDescription
from launch.actions import ExecuteProcess
from ament_index_python.packages import get_package_prefix
import os


def generate_launch_description():
    exe = os.path.join(
        get_package_prefix('todo_login'), 'lib', 'todo_login', 'todo_login')

    return LaunchDescription([
        ExecuteProcess(
            cmd=[exe],
            name='todo_login',
            output='screen',
            emulate_tty=True,
        ),
    ])
