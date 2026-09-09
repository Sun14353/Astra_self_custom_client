"""
doorlock.launch.py —— ROS2 Humble 版本

将大恒相机节点（Daheng Camera）和门锁视频编码节点（doorlock video encoder）
组合到同一个进程内（ComposableNodeContainer）运行，
启用 ROS2 Intra-Process Communication（进程内通信），
利用 Zero-Copy（零拷贝）机制减少数据复制，提高图像传输性能。

同时，将 Python 编写的视频解码节点作为独立进程启动。
"""

from launch import LaunchDescription
from launch_ros.actions import ComposableNodeContainer, Node
from launch_ros.descriptions import ComposableNode
from pathlib import Path
from ament_index_python.packages import get_package_share_directory

def generate_launch_description():

    # ROS2 官方方式获取 bringup 包 share 目录
    bringup_share = Path(get_package_share_directory("bringup"))

    # share/bringup -> share -> install -> workspace
    workspace_root = bringup_share.parents[2]

    debug_dump_dir = str(workspace_root / "sniper_debug_imgs")

 # 调试图片导出开关
    debug_dump_enable = False        # 是否启用调试图片保存
    debug_dump_every_n_frames = 1    # 每隔 N 帧保存一次图片

# 编码器各阶段图像保存选项
    dump_save_raw = False            # 保存编码器输入的原始图像（Raw）
    dump_save_roi = True             # 保存 ROI（感兴趣区域）图像
    dump_save_static = False         # 保存静态背景/静态区域处理结果
    dump_save_final = True           # 保存最终送入编码器的图像

# 解码器图像保存选项
    dump_save_decoder = True         # 保存解码后的显示图像

# 传输图像相关参数
    target_bitrate_kbytes = 10.0   
    hard_max_bitrate_kbytes = 14.0  
    target_bitrate_kbps = int(target_bitrate_kbytes * 8.0)
    x264_preset = 'veryslow'        
    encode_size = 300

    encoder_container = ComposableNodeContainer(
        name='doorlock_container',
        namespace='',
        package='rclcpp_components',
        executable='component_container',
        composable_node_descriptions=[
            ComposableNode(
                package='daheng_camera',
                plugin='daheng_camera::DahengCameraNode',
                name='daheng_camera',
                parameters=[
                    {'exposure_time': 10000.0},  # us
                    {'gain': 5.0},
                    {'frame_rate': 60.0},
                    {'use_sensor_data_qos': True},
                    {'camera_info_url': 'package://daheng_camera/config/camera_6mm_MER-031-300GC.yaml'},
                ],
                extra_arguments=[{'use_intra_process_comms': True}]
            ),
            ComposableNode(
                package='doorlock_sniper',
                plugin='doorlock_sniper::VideoEncoderNode',
                name='video_encoder',
                parameters=[
                    {'input_topic': '/image_raw'},
                    {'target_bitrate': target_bitrate_kbps},
                    {'x264_preset': x264_preset},
                    {'output_fps': 60},
                    {'packet_size': 300},
                    {'enable_display': True},
                    {'debug_dump_enable': debug_dump_enable},
                    {'debug_dump_every_n_frames': debug_dump_every_n_frames},
                    {'debug_dump_save_raw': dump_save_raw},
                    {'debug_dump_save_roi': dump_save_roi},
                    {'debug_dump_save_static': dump_save_static},
                    {'debug_dump_save_final': dump_save_final},
                    {'debug_dump_dir': debug_dump_dir},
                    {'crop_size': 800},
                    {'output_size': encode_size},
                    {'static_simplify': True},
                    {'motion_threshold': 14},
                    {'motion_erode_px': 2},
                    {'motion_dilate_px': 6},
                    {'motion_trail_frames': 90},
                    {'trail_disable_motion_ratio': 0.30},
                    {'bg_update_alpha': 0.01},
                    {'bg_blur_sigma': 1.8},
                    {'center_clear_size': 150},
                    {'force_monochrome': False},
                    {'bandwidth_limit_kbytes': hard_max_bitrate_kbytes},
                    {'bandwidth_window_s': 2.0},
                    {'max_tx_delay_s': 1.0},
                ],
                extra_arguments=[{'use_intra_process_comms': True}]
            ),
        ],
        output='screen',
    )

    decoder_node = Node(
        package='doorlock_decoder',
        executable='decoder_node',
        name='video_decoder',
        parameters=[
            {'topic': '/video_stream'},
            {'display': True},
            {'width': encode_size},
            {'height': encode_size},
            {'display_scale': 2},
            {'crosshair_offset_x': 0},
            {'crosshair_offset_y': 0},
            {'crosshair_width': 1},
            {'debug_dump_enable': debug_dump_enable},
            {'debug_dump_every_n_frames': debug_dump_every_n_frames},
            {'debug_dump_save_decoder': dump_save_decoder},
            {'debug_dump_dir': debug_dump_dir},
        ],
        output='screen',
        emulate_tty=True,
    )

    return LaunchDescription([
        encoder_container,
        decoder_node,
    ])
