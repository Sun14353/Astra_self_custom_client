cmake_minimum_required(VERSION 3.8)
project(todo_presets)

find_package(ament_cmake REQUIRED)

# 纯配置文件包：安装 presets 目录到 share/todo_presets/presets。
install(DIRECTORY presets
  DESTINATION share/${PROJECT_NAME}/
)

ament_package()
