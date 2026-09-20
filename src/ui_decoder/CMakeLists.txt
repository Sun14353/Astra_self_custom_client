cmake_minimum_required(VERSION 3.8)
project(ui_decoder)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -O2)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(doorlock_sniper REQUIRED)
find_package(std_msgs REQUIRED)

# Qt5 (系统 5.15.3)。避免误用 Anaconda 自带的 Qt 5.15.2:
# 显式强制系统 Qt5 的 cmake 配置目录 (CACHE FORCE 覆盖 find_package 的
# 自动发现，防止 conda base 环境里的 Anaconda Qt 被选中)。
set(Qt5_DIR "/usr/lib/x86_64-linux-gnu/cmake/Qt5" CACHE PATH "Qt5 dir" FORCE)
set(Qt5Core_DIR "/usr/lib/x86_64-linux-gnu/cmake/Qt5Core" CACHE PATH "Qt5Core dir" FORCE)
set(Qt5Gui_DIR "/usr/lib/x86_64-linux-gnu/cmake/Qt5Gui" CACHE PATH "Qt5Gui dir" FORCE)
set(Qt5Widgets_DIR "/usr/lib/x86_64-linux-gnu/cmake/Qt5Widgets" CACHE PATH "Qt5Widgets dir" FORCE)
find_package(Qt5 REQUIRED COMPONENTS Widgets)

# FFmpeg 解码 (系统 libavcodec 58 / libavutil 56 / libswscale 5)
find_package(PkgConfig REQUIRED)
pkg_check_modules(AVCODEC REQUIRED libavcodec)
pkg_check_modules(AVUTIL REQUIRED libavutil)
pkg_check_modules(SWSCALE REQUIRED libswscale)

# Q_OBJECT 需要 moc
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_executable(ui_decoder_node
  src/main.cpp
  src/ui_decoder_node.cpp
  src/widgets/CrosshairWidget.cpp
  src/widgets/HeatRingWidget.cpp
  src/widgets/VideoBackgroundWidget.cpp
  src/network/H264Decoder.cpp
  src/network/H264AnnexB.cpp
)

target_compile_features(ui_decoder_node PRIVATE cxx_std_17)

target_include_directories(ui_decoder_node PRIVATE
  src
  src/widgets
  src/network
)

ament_target_dependencies(ui_decoder_node
  rclcpp
  doorlock_sniper
  std_msgs
)

# 链接生成的消息类型支持 (doorlock_sniper/VideoPacket)。
# 下游消费消息包时不能调用 rosidl_get_typesupport_target（该函数只在本包内
# rosidl_generate_interfaces 创建 target 后有效），应直接链接导出的
# namespaced typesupport target。
target_link_libraries(ui_decoder_node
  doorlock_sniper::doorlock_sniper__rosidl_typesupport_cpp
)

target_include_directories(ui_decoder_node PRIVATE
  ${AVCODEC_INCLUDE_DIRS}
  ${AVUTIL_INCLUDE_DIRS}
  ${SWSCALE_INCLUDE_DIRS}
)
target_link_libraries(ui_decoder_node
  Qt5::Widgets
  ${AVCODEC_LIBRARIES}
  ${AVUTIL_LIBRARIES}
  ${SWSCALE_LIBRARIES}
)

# 运行时 rpath: 强制使用系统 Qt5 / FFmpeg, 并关闭 new-dtags 使其对
# 传递依赖同样生效 (与 doorlock_sniper 一致的做法)。
if(NOT WIN32)
  target_link_options(ui_decoder_node PRIVATE "-Wl,--disable-new-dtags")
endif()

install(TARGETS ui_decoder_node
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY launch
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
