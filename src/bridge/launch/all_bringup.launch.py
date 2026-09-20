"""
all_bringup.launch.py -- 项目级总启动入口

同时启动:
  1) 通信/图传链路端 (bridge.launch.py)         <-- 通信端
  2) 发射端 (bringup/launch/doorlock.launch.py)  <-- ROS 2 相机+编码+解码
  3) UI 解码端 (ui_decoder/launch/ui_decoder.launch.py) <-- Qt5 瞄准 UI
"""

from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from ament_index_python.packages import get_package_share_directory
import os


def generate_launch_description():
    # 1) 通信端: include bridge 自身的 bridge.launch.py
    bridge_share = get_package_share_directory('bridge')
    bridge_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(bridge_share, 'launch', 'bridge.launch.py')
        ),
        launch_arguments={'robot_id': '1'}.items(),
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
        )
    )

    return LaunchDescription([
        bridge_launch,
        doorlock_launch,
        ui_decoder_launch,
    ])
