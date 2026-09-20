cmake_minimum_required(VERSION 3.16)
project(bridge LANGUAGES CXX)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra)
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------- ROS 2 依赖 ----------------
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)

# ---------------- Protobuf 25.1 用户自定义安装路径 ----------------
set(USER_PROTOBUF_PREFIX "$ENV{HOME}/protobuf")
if(EXISTS "${USER_PROTOBUF_PREFIX}/lib/cmake/protobuf/protobuf-config.cmake")
  message(STATUS "bridge: using user-installed Protobuf at ${USER_PROTOBUF_PREFIX}")
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
    COMMENT "bridge: generating protobuf ${proto_name}"
    VERBATIM
  )
  list(APPEND PROTO_SRCS ${PB_CC})
endforeach()

# ---------------- 共享库: bridge_core (业务逻辑) ----------------
# bridge_client.cpp / bridge_node.cpp 都会链接这个
add_library(bridge_core STATIC
  src/bridge_mqtt_client.cpp
  src/udp_video_receiver.cpp
  ${PROTO_SRCS}
)

target_include_directories(bridge_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${PROTO_GEN_DIR}
)

if(TARGET protobuf::libprotobuf)
  set(PROTOBUF_LIB_TGT protobuf::libprotobuf)
else()
  set(PROTOBUF_LIB_TGT ${Protobuf_LIBRARIES})
endif()

if(TARGET PahoMqttCpp::paho-mqttpp)
  target_link_libraries(bridge_core PUBLIC
    PahoMqttCpp::paho-mqttpp
    ${PROTOBUF_LIB_TGT}
  )
else()
  target_link_libraries(bridge_core PUBLIC
    paho-mqttpp3
    paho-mqtt3a
    ${PROTOBUF_LIB_TGT}
  )
endif()
target_link_libraries(bridge_core PUBLIC Threads::Threads)

# ---------------- 可执行: bridge_client (纯 C++, 由 bridge_node 启动) ----------------
add_executable(bridge_client src/bridge_client.cpp)
target_link_libraries(bridge_client PRIVATE bridge_core)

# rpath: 运行时找 abseil 库
if(NOT WIN32)
  set_target_properties(bridge_client PROPERTIES
    BUILD_RPATH "${USER_PROTOBUF_PREFIX}/lib"
    INSTALL_RPATH "${USER_PROTOBUF_PREFIX}/lib"
  )
  target_link_options(bridge_client PRIVATE "-Wl,--disable-new-dtags")
endif()

# ---------------- 可执行: match_simulator (赛场信息发布模拟器) ----------------
add_executable(match_simulator src/match_simulator.cpp)
target_link_libraries(match_simulator PRIVATE bridge_core)

if(NOT WIN32)
  target_link_options(match_simulator PRIVATE "-Wl,--disable-new-dtags")
endif()

# ---------------- 可执行: bridge_node (ROS 2 节点, 启动并管理 bridge_client) ----------------
add_executable(bridge_node src/bridge_node.cpp)
target_link_libraries(bridge_node PRIVATE
  bridge_core
  rclcpp::rclcpp
)

if(NOT WIN32)
  target_link_options(bridge_node PRIVATE "-Wl,--disable-new-dtags")
endif()

# ---------------- Install ----------------
install(TARGETS
  bridge_client
  bridge_node
  match_simulator
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY launch DESTINATION share/${PROJECT_NAME}/)

ament_package()
