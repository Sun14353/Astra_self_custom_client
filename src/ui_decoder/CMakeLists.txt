cmake_minimum_required(VERSION 3.8)
project(ui_decoder)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -O2)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(doorlock_sniper REQUIRED)
find_package(std_msgs REQUIRED)

# ---------------- Protobuf 25.1 用户自定义安装路径 ----------------
# 与 bridge 包保持一致：优先使用 ~/protobuf 下自装的 protobuf。
set(USER_PROTOBUF_PREFIX "$ENV{HOME}/protobuf")
if(EXISTS "${USER_PROTOBUF_PREFIX}/lib/cmake/protobuf/protobuf-config.cmake")
  message(STATUS "ui_decoder: using user-installed Protobuf at ${USER_PROTOBUF_PREFIX}")
  list(PREPEND CMAKE_PREFIX_PATH "${USER_PROTOBUF_PREFIX}")
  set(Protobuf_PROTOC_EXECUTABLE "${USER_PROTOBUF_PREFIX}/bin/protoc")
  set(USER_PROTOBUF_USED TRUE)
endif()

find_package(Protobuf CONFIG QUIET)
if(NOT Protobuf_FOUND)
  find_package(Protobuf REQUIRED)
endif()
if(USER_PROTOBUF_USED)
  set(Protobuf_PROTOC_EXECUTABLE "${USER_PROTOBUF_PREFIX}/bin/protoc")
endif()

# ---------------- Paho MQTT C++ ----------------
find_package(PahoMqttCpp QUIET)

# ---------------- 从 .proto 生成 C++ ----------------
set(PROTO_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/src/proto)
set(PROTO_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/proto_gen)
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})

set(PROTO_FILES ${PROTO_DIR}/custom_client.proto)
set(PROTO_SRCS)
foreach(proto ${PROTO_FILES})
  get_filename_component(proto_name ${proto} NAME_WE)
  set(PB_CC ${PROTO_GEN_DIR}/${proto_name}.pb.cc)
  set(PB_H  ${PROTO_GEN_DIR}/${proto_name}.pb.h)
  add_custom_command(
    OUTPUT  ${PB_CC} ${PB_H}
    COMMAND ${CMAKE_COMMAND} -E env
            "LD_LIBRARY_PATH=${USER_PROTOBUF_PREFIX}/lib:$ENV{LD_LIBRARY_PATH}"
            ${Protobuf_PROTOC_EXECUTABLE}
            --cpp_out=${PROTO_GEN_DIR}
            --proto_path=${PROTO_DIR}
            ${proto}
    DEPENDS ${proto}
    COMMENT "ui_decoder: generating protobuf ${proto_name}"
    VERBATIM
  )
  list(APPEND PROTO_SRCS ${PB_CC})
endforeach()

# Qt5 (系统 5.15.3)。避免误用 Anaconda 自带的 Qt 5.15.2:
# 显式强制系统 Qt5 的 cmake 配置目录 (CACHE FORCE 覆盖 find_package 的
# 自动发现，防止 conda base 环境里的 Anaconda Qt 被选中)。
set(Qt5_DIR "/usr/lib/x86_64-linux-gnu/cmake/Qt5" CACHE PATH "Qt5 dir" FORCE)
set(Qt5Core_DIR "/usr/lib/x86_64-linux-gnu/cmake/Qt5Core" CACHE PATH "Qt5Core dir" FORCE)
set(Qt5Gui_DIR "/usr/lib/x86_64-linux-gnu/cmake/Qt5Gui" CACHE PATH "Qt5Gui dir" FORCE)
set(Qt5Widgets_DIR "/usr/lib/x86_64-linux-gnu/cmake/Qt5Widgets" CACHE PATH "Qt5Widgets dir" FORCE)
set(Qt5Qml_DIR "/usr/lib/x86_64-linux-gnu/cmake/Qt5Qml" CACHE PATH "Qt5Qml dir" FORCE)
set(Qt5Quick_DIR "/usr/lib/x86_64-linux-gnu/cmake/Qt5Quick" CACHE PATH "Qt5Quick dir" FORCE)
set(Qt5QuickWidgets_DIR "/usr/lib/x86_64-linux-gnu/cmake/Qt5QuickWidgets" CACHE PATH "Qt5QuickWidgets dir" FORCE)
find_package(Qt5 REQUIRED COMPONENTS Widgets Qml Quick QuickWidgets)

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
  src/MatchDataProvider.cpp
  src/widgets/CrosshairWidget.cpp
  src/widgets/HeatRingWidget.cpp
  src/widgets/VideoBackgroundWidget.cpp
  src/network/H264Decoder.cpp
  src/network/H264AnnexB.cpp
  src/network/MyMqttClient.cpp
  ${PROTO_SRCS}
  src/qml/qml.qrc
)

target_compile_features(ui_decoder_node PRIVATE cxx_std_17)

target_include_directories(ui_decoder_node PRIVATE
  src
  src/widgets
  src/network
  ${PROTO_GEN_DIR}
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
  Qt5::Qml
  Qt5::Quick
  Qt5::QuickWidgets
  ${AVCODEC_LIBRARIES}
  ${AVUTIL_LIBRARIES}
  ${SWSCALE_LIBRARIES}
)

# ---------------- Protobuf + Paho 链接 ----------------
if(TARGET protobuf::libprotobuf)
  set(PROTOBUF_LIB_TGT protobuf::libprotobuf)
else()
  set(PROTOBUF_LIB_TGT ${Protobuf_LIBRARIES})
endif()

if(TARGET PahoMqttCpp::paho-mqttpp)
  target_link_libraries(ui_decoder_node
    PahoMqttCpp::paho-mqttpp
    ${PROTOBUF_LIB_TGT}
  )
else()
  target_link_libraries(ui_decoder_node
    paho-mqttpp3
    paho-mqtt3a
    ${PROTOBUF_LIB_TGT}
  )
endif()
target_link_libraries(ui_decoder_node Threads::Threads)

# 运行时 rpath: 强制使用系统 Qt5 / FFmpeg / protobuf, 并关闭 new-dtags 使其对
# 传递依赖同样生效 (与 bridge/doorlock_sniper 一致的做法)。
if(NOT WIN32)
  target_link_options(ui_decoder_node PRIVATE "-Wl,--disable-new-dtags")
  set_target_properties(ui_decoder_node PROPERTIES
    BUILD_RPATH "${USER_PROTOBUF_PREFIX}/lib"
    INSTALL_RPATH "${USER_PROTOBUF_PREFIX}/lib"
  )
endif()

install(TARGETS ui_decoder_node
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY launch
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
