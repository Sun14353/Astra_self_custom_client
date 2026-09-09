cmake_minimum_required(VERSION 3.8)
project(daheng_camera)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

# =======================================
# 依赖
# =======================================

find_package(ament_cmake REQUIRED)
find_package(OpenCV 4 REQUIRED COMPONENTS core imgproc)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(image_transport REQUIRED)
find_package(camera_info_manager REQUIRED)

# =======================================
# 大恒 Galaxy SDK 路径配置
# 默认指向工作空间根目录下的 Galaxy_camera/{inc,lib}（即 src/daheng_camera 的上两级父目录）。
# 编译时可通过 -D 覆盖：
#   colcon build --cmake-args -DDAHENG_INCLUDE_DIR=/path/to/inc \
#                            -DDAHENG_LIB_DIR=/path/to/lib
# =======================================

set(DAHENG_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../Galaxy_camera/inc" CACHE PATH "Daheng Galaxy SDK include directory")
set(DAHENG_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../Galaxy_camera/lib" CACHE PATH "Daheng Galaxy SDK library directory")

if(NOT EXISTS "${DAHENG_INCLUDE_DIR}")
  message(WARNING "DAHENG_INCLUDE_DIR (${DAHENG_INCLUDE_DIR}) does not exist. "
                  "Set -DDAHENG_INCLUDE_DIR=... when invoking colcon build.")
endif()

if(NOT EXISTS "${DAHENG_LIB_DIR}")
  message(WARNING "DAHENG_LIB_DIR (${DAHENG_LIB_DIR}) does not exist. "
                  "Set -DDAHENG_LIB_DIR=... when invoking colcon build.")
endif()

# =======================================
# 包含目录
# =======================================

include_directories(
  include
  ${DAHENG_INCLUDE_DIR}
  ${OpenCV_INCLUDE_DIRS}
)

# =======================================
# 构建组件库
# =======================================

add_library(daheng_camera_component SHARED
  src/daheng_camera_node.cpp
)

target_compile_features(daheng_camera_component PUBLIC cxx_std_17)

target_link_directories(daheng_camera_component PRIVATE ${DAHENG_LIB_DIR})

# DxImageProc 头文件里的函数（DxRaw8toRGB24 等）由 libgxiapi.so 一并提供，
# 所以只需链接 -lgxiapi。如果你的 SDK 安装包是分开的，加回 -ldxvideoproc 即可。
target_link_libraries(daheng_camera_component PUBLIC
  rclcpp::rclcpp
  rclcpp_components::component
  sensor_msgs::sensor_msgs_library
  image_transport::image_transport
  camera_info_manager::camera_info_manager
  ${OpenCV_LIBRARIES}

  # 大恒 Galaxy SDK
  gxiapi
)

# 让运行时能找到 SDK 共享库
set_target_properties(daheng_camera_component PROPERTIES
  INSTALL_RPATH "${DAHENG_LIB_DIR}"
  BUILD_RPATH "${DAHENG_LIB_DIR}"
)

rclcpp_components_register_nodes(daheng_camera_component "daheng_camera::DahengCameraNode")

# =======================================
# 安装
# =======================================

install(TARGETS
  daheng_camera_component
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(DIRECTORY
  config
  DESTINATION share/${PROJECT_NAME}
)

install(DIRECTORY
  launch
  DESTINATION share/${PROJECT_NAME}
)

# =======================================
# 导出依赖
# =======================================

ament_export_dependencies(
  rclcpp
  rclcpp_components
  sensor_msgs
  image_transport
  camera_info_manager
)

ament_package()
