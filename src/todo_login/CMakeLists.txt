cmake_minimum_required(VERSION 3.8)
project(todo_login)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra)
endif()

find_package(ament_cmake REQUIRED)

# Qt5 (系统 5.15.3)。与 ui_decoder 一致：显式强制系统 Qt5 的 cmake 配置目录，
# 避免误用 Anaconda 自带的 Qt 5.15.2。
set(Qt5_DIR "/usr/lib/x86_64-linux-gnu/cmake/Qt5" CACHE PATH "Qt5 dir" FORCE)
set(Qt5Core_DIR "/usr/lib/x86_64-linux-gnu/cmake/Qt5Core" CACHE PATH "Qt5Core dir" FORCE)
set(Qt5Gui_DIR "/usr/lib/x86_64-linux-gnu/cmake/Qt5Gui" CACHE PATH "Qt5Gui dir" FORCE)
set(Qt5Widgets_DIR "/usr/lib/x86_64-linux-gnu/cmake/Qt5Widgets" CACHE PATH "Qt5Widgets dir" FORCE)
set(Qt5Qml_DIR "/usr/lib/x86_64-linux-gnu/cmake/Qt5Qml" CACHE PATH "Qt5Qml dir" FORCE)
set(Qt5Quick_DIR "/usr/lib/x86_64-linux-gnu/cmake/Qt5Quick" CACHE PATH "Qt5Quick dir" FORCE)
set(Qt5QuickWidgets_DIR "/usr/lib/x86_64-linux-gnu/cmake/Qt5QuickWidgets" CACHE PATH "Qt5QuickWidgets dir" FORCE)
find_package(Qt5 REQUIRED COMPONENTS Widgets Qml Quick QuickWidgets)

# Q_OBJECT / qrc 需要 moc 与 rcc
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_executable(todo_login
  src/main.cpp
  src/player_profile_model.h
  src/player_profile_model.cpp
  qml/qml.qrc
)

target_include_directories(todo_login PRIVATE src)

target_link_libraries(todo_login
  Qt5::Widgets
  Qt5::Qml
  Qt5::Quick
  Qt5::QuickWidgets
)

install(TARGETS todo_login
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY launch
  DESTINATION share/${PROJECT_NAME}/
)

ament_package()
