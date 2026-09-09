// SPDX-License-Identifier: MIT
//
// DahengCameraNode - ROS 2 Humble wrapper for Daheng Imaging Galaxy SDK.
//
// Compared with the Hikvision MVS SDK version (Pacific_doorlock_sniper), this
// driver uses Daheng's GxIAPI. DxRaw8toRGB24 (which converts raw Bayer to
// RGB24) is also exported from libgxiapi.so, so a single -lgxiapi link covers
// everything the driver needs.
//
// Important SDK concepts:
//   * GX_DEV_HANDLE  - device handle returned by GXOpenDevice / GXOpenDeviceByIndex
//   * GX_DS_HANDLE   - data stream handle returned by GXGetDataStreamHandleFromDev
//   * GX_FRAME_DATA_EX - per-frame payload returned by GXDQBufEx
//   * GX_PIXEL_COLOR_FILTER_ENTRY - Bayer pattern (RG/GB/GR/BG) used by DxRaw8toRGB24
//   * GX_PIXEL_FORMAT_BAYER_RG8 - pixel format we configure the device into

#ifndef DAHENG_CAMERA_DAHENG_CAMERA_NODE_HPP_
#define DAHENG_CAMERA_DAHENG_CAMERA_NODE_HPP_

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/camera_info.hpp>
#include <sensor_msgs/msg/image.hpp>

#include <camera_info_manager/camera_info_manager.hpp>
#include <image_transport/image_transport.hpp>

namespace daheng_camera
{

class DahengCameraNode : public rclcpp::Node
{
public:
  explicit DahengCameraNode(const rclcpp::NodeOptions & options);
  ~DahengCameraNode() override;

private:
  // -------- SDK lifecycle helpers --------
  bool initialize_sdk();
  void shutdown_sdk();
  bool open_first_device();
  void close_device();
  void declare_parameters();
  void apply_parameters();

  // -------- Capture worker --------
  void capture_loop();

  // -------- Parameter callback --------
  rcl_interfaces::msg::SetParametersResult on_parameter_change(
    const std::vector<rclcpp::Parameter> & parameters);

  // -------- ROS plumbing --------
  sensor_msgs::msg::Image image_msg_;
  sensor_msgs::msg::CameraInfo camera_info_msg_;
  image_transport::CameraPublisher camera_pub_;
  std::string camera_name_;
  std::unique_ptr<camera_info_manager::CameraInfoManager> camera_info_manager_;

  // Worker thread that drives GxIAPI.
  std::thread capture_thread_;
  std::atomic<bool> running_;

  // Daheng SDK handles stored as void*; the actual types (GX_DEV_HANDLE and
  // GX_DS_HANDLE) are defined in <GxIAPI.h>. We use void* here to keep this
  // header independent of the SDK include path (the .cpp includes it).
  void * device_handle_;
  void * ds_handle_;

  // Configurable camera parameters
  double exposure_time_us_;
  double gain_;
  bool use_sensor_data_qos_;
  int pixel_format_bayer_;  // GX_PIXEL_COLOR_FILTER_ENTRY

  // Parameter callback handle.
  OnSetParametersCallbackHandle::SharedPtr param_cb_handle_;
};

}  // namespace daheng_camera

#endif  // DAHENG_CAMERA_DAHENG_CAMERA_NODE_HPP_
