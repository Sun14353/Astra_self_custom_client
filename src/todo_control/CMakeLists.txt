cmake_minimum_required(VERSION 3.8)
project(todo_control)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)

# ============================================================
# 架构骨架：自定义控制节点。
# 当前仅一个占位节点 + 控制管理器桩，实际控制逻辑 TODO。
# ============================================================
add_executable(todo_control_node
  src/todo_control_node.cpp
  src/control_manager.cpp
)
target_include_directories(todo_control_node PRIVATE include)
ament_target_dependencies(todo_control_node rclcpp)

install(TARGETS todo_control_node
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY include/ DESTINATION include/)
install(DIRECTORY config launch
  DESTINATION share/${PROJECT_NAME}/
)

ament_package()
