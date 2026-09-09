// SPDX-License-Identifier: MIT
//
// DahengCameraNode —— ROS 2 Humble 版本的大恒（Daheng Imaging）Galaxy SDK 相机驱动。
//
// SDK 安装位置：
//   - 头文件：/opt/galaxy
//   - 动态库：/usr/lib
//
// Galaxy SDK 的所有接口（GxIAPI）以及图像处理函数
// （如 DxRaw8toRGB24）均由 libgxiapi.so 导出，
// 因此编译时只需链接一个库：-lgxiapi。
//
// 相机采集流程：
//
//   GXInitLib()
//       初始化 Galaxy SDK。
//
//   GXUpdateAllDeviceList(&n, 1000)
//       扫描并更新设备列表，等待最多 1000 ms。
//
//   GXOpenDeviceByIndex(1, &hDev)
//       按设备序号打开相机（也可使用 GXOpenDevice() 按 SN 或 IP 打开）。
//
//   GXSetEnumValueByString(hDev, "PixelFormat", "BayerRG8")
//       设置相机输出像素格式为 BayerRG8。
//
//   GXSetAcqusitionBufferNumber(hDev, 5)
//       设置 SDK 内部采集缓冲区数量为 5。
//
//   GXSetEnumValueByString(hDev, "AcquisitionMode", "Continuous")
//       设置采集模式为连续采集。
//
//   GXSetEnumValueByString(hDev, "TriggerMode", "Off")
//       关闭触发模式，采用自由连续采集。
//
//   GXStreamOn(hDev)
//       开始图像采集。
//
//   循环执行：
//       GXDQBufEx(hDev, &pFrame, 1000)
//           从 SDK 获取一帧图像（阻塞等待，超时时间 1000 ms）。
//
//       DxRaw8toRGB24(...)
//           将 BayerRG8 原始图像去马赛克（Demosaic），
//           转换为 RGB24 彩色图像。
//
//       发布 sensor_msgs/msg/Image
//           将 RGB 图像封装为 ROS2 Image 消息并发布。
//
//       GXQBufEx(hDev, pFrame)
//           将当前图像缓冲区归还给 SDK，供下一帧继续使用。
//
//   GXStreamOff(hDev)
//       停止图像采集。
//
//   GXCloseDevice(hDev)
//       关闭相机设备。
//
//   GXCloseLib()
//       释放 Galaxy SDK，程序退出。

#include "daheng_camera/daheng_camera_node.hpp"

#include <cstring>
#include <functional>
#include <thread>

#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>

#include <rclcpp/logging.hpp>

// Daheng Galaxy SDK (installed at /opt/galaxy)
#include <DxImageProc.h>
#include <GxIAPI.h>

namespace daheng_camera
{

namespace
{
void log_gx_error(rclcpp::Logger logger, GX_STATUS status, const char * where)
{
  if (status == GX_STATUS_SUCCESS) {
    return;
  }
  size_t size = 0;
  GX_STATUS le = GXGetLastError(&status, nullptr, &size);
  std::string text;
  if (le == GX_STATUS_SUCCESS && size > 0) {
    text.resize(size);
    le = GXGetLastError(&status, text.data(), &size);
    if (le != GX_STATUS_SUCCESS) {
      text.clear();
    }
  }
  RCLCPP_ERROR(logger,
    "\x1b[1;31m[Daheng] %s failed: status=0x%08x %s\x1b[0m",
    where, static_cast<unsigned int>(status), text.c_str());
}
}  // namespace

DahengCameraNode::DahengCameraNode(const rclcpp::NodeOptions & options)
: rclcpp::Node("daheng_camera", options),
  device_handle_(nullptr),
  ds_handle_(nullptr),
  exposure_time_us_(0.0),
  gain_(0.0),
  use_sensor_data_qos_(true),
  pixel_format_bayer_(GX_PIXEL_COLOR_FILTER_ENTRY::GX_COLOR_FILTER_BAYER_RG)
{
  RCLCPP_INFO(this->get_logger(),
    "Starting DahengCameraNode (Galaxy SDK, ROS 2 Humble)");

  declare_parameters();

  // QoS for image transport
  rmw_qos_profile_t qos_profile = use_sensor_data_qos_
    ? rmw_qos_profile_sensor_data
    : rmw_qos_profile_default;
  camera_pub_ = image_transport::create_camera_publisher(this, "image_raw", qos_profile);

  // Camera info manager (load intrinsics YAML if available)
  camera_name_ = this->declare_parameter("camera_name", std::string("daheng_camera"));
  camera_info_manager_ =
    std::make_unique<camera_info_manager::CameraInfoManager>(this, camera_name_);
  const auto camera_info_url = this->declare_parameter(
    "camera_info_url",
    std::string("package://daheng_camera/config/camera_6mm_MER-031-300GC.yaml"));
  if (camera_info_manager_->validateURL(camera_info_url)) {
    camera_info_manager_->loadCameraInfo(camera_info_url);
    camera_info_msg_ = camera_info_manager_->getCameraInfo();
  } else {
    RCLCPP_WARN(this->get_logger(), "Invalid camera info URL: %s", camera_info_url.c_str());
  }

  param_cb_handle_ = this->add_on_set_parameters_callback(
    std::bind(&DahengCameraNode::on_parameter_change, this, std::placeholders::_1));

  if (!initialize_sdk() || !open_first_device()) {
    RCLCPP_ERROR(this->get_logger(),
      "\x1b[1;31m[Daheng] Failed to open a camera. Capture thread will keep retrying.\x1b[0m");
  }

  apply_parameters();

  running_.store(true);
  capture_thread_ = std::thread(&DahengCameraNode::capture_loop, this);
}

DahengCameraNode::~DahengCameraNode()
{
  running_.store(false);
  if (capture_thread_.joinable()) {
    capture_thread_.join();
  }
  close_device();
  shutdown_sdk();
  RCLCPP_INFO(this->get_logger(), "DahengCameraNode destroyed");
}

void DahengCameraNode::declare_parameters()
{
  this->declare_parameter("exposure_time", 10000.0);  // microseconds
  this->declare_parameter("gain", 5.0);
  this->declare_parameter("use_sensor_data_qos", true);
  this->declare_parameter("roi_width", 1440);
  this->declare_parameter("roi_height", 1080);
  this->declare_parameter("frame_rate", 60.0);
  this->declare_parameter("acquisition_buffer_number", 5);
  // Bayer pattern: 1=RG, 2=GB, 3=GR, 4=BG. The default "RG" is the most common.
  this->declare_parameter("bayer_pattern", 1);
}

void DahengCameraNode::apply_parameters()
{
  this->get_parameter("exposure_time", exposure_time_us_);
  this->get_parameter("gain", gain_);
  this->get_parameter("use_sensor_data_qos", use_sensor_data_qos_);
  int bayer_idx = 1;
  this->get_parameter("bayer_pattern", bayer_idx);
  pixel_format_bayer_ = static_cast<GX_PIXEL_COLOR_FILTER_ENTRY>(bayer_idx);

  if (!device_handle_) {
    return;
  }
  GX_DEV_HANDLE h = static_cast<GX_DEV_HANDLE>(device_handle_);

  // Exposure (microseconds, raw float value on the device)
  GX_STATUS s = GXSetFloatValue(h, "ExposureTime", exposure_time_us_);
  log_gx_error(this->get_logger(), s, "GXSetFloatValue(ExposureTime)");

  // Gain
  s = GXSetFloatValue(h, "Gain", gain_);
  log_gx_error(this->get_logger(), s, "GXSetFloatValue(Gain)");

  // ROI size
  int64_t width = 0, height = 0;
  this->get_parameter("roi_width", width);
  this->get_parameter("roi_height", height);
  if (width > 0) {
    s = GXSetIntValue(h, "Width", width);
    log_gx_error(this->get_logger(), s, "GXSetIntValue(Width)");
  }
  if (height > 0) {
    s = GXSetIntValue(h, "Height", height);
    log_gx_error(this->get_logger(), s, "GXSetIntValue(Height)");
  }

  // Frame rate (AcquisitionFrameRate)
  double fps = 0.0;
  this->get_parameter("frame_rate", fps);
  if (fps > 0.0) {
    s = GXSetFloatValue(h, "AcquisitionFrameRate", fps);
    log_gx_error(this->get_logger(), s, "GXSetFloatValue(AcquisitionFrameRate)");
  }

  RCLCPP_INFO(this->get_logger(),
    "Applied parameters: exposure=%.1fus gain=%.2f fps=%.1f bayer=%d",
    exposure_time_us_, gain_, fps, bayer_idx);
}

bool DahengCameraNode::initialize_sdk()
{
  GX_STATUS s = GXInitLib();
  if (s != GX_STATUS_SUCCESS) {
    log_gx_error(this->get_logger(), s, "GXInitLib");
    return false;
  }
  return true;
}

void DahengCameraNode::shutdown_sdk()
{
  GXCloseLib();
}

bool DahengCameraNode::open_first_device()
{
  uint32_t num_devices = 0;
  GX_STATUS s = GXUpdateAllDeviceList(&num_devices, 1000);
  if (s != GX_STATUS_SUCCESS) {
    log_gx_error(this->get_logger(), s, "GXUpdateAllDeviceList");
    return false;
  }
  if (num_devices == 0) {
    RCLCPP_ERROR(this->get_logger(), "[Daheng] No device found");
    return false;
  }
  RCLCPP_INFO(this->get_logger(), "[Daheng] Found %u device(s)", num_devices);

  GX_DEV_HANDLE h = nullptr;
  s = GXOpenDeviceByIndex(1, &h);
  if (s != GX_STATUS_SUCCESS || h == nullptr) {
    log_gx_error(this->get_logger(), s, "GXOpenDeviceByIndex(1)");
    return false;
  }
  device_handle_ = h;

  // Print the device info banner.
  GX_STRING_VALUE sv{};
  if (GXGetStringValue(h, "DeviceVendorName", &sv) == GX_STATUS_SUCCESS) {
    RCLCPP_INFO(this->get_logger(), "  Vendor : %s", sv.strCurValue);
  }
  if (GXGetStringValue(h, "DeviceModelName", &sv) == GX_STATUS_SUCCESS) {
    RCLCPP_INFO(this->get_logger(), "  Model  : %s", sv.strCurValue);
  }
  if (GXGetStringValue(h, "DeviceSerialNumber", &sv) == GX_STATUS_SUCCESS) {
    RCLCPP_INFO(this->get_logger(), "  Serial : %s", sv.strCurValue);
  }
  RCLCPP_INFO(this->get_logger(), "  Library: %s", GXGetLibVersion());

  // Configure the device for software-triggered continuous capture.
  // Set acquisition mode = Continuous
  s = GXSetEnumValueByString(h, "AcquisitionMode", "Continuous");
  log_gx_error(this->get_logger(), s, "Set AcquisitionMode=Continuous");

  // Disable trigger (free-run)
  s = GXSetEnumValueByString(h, "TriggerMode", "Off");
  log_gx_error(this->get_logger(), s, "Set TriggerMode=Off");

  // Try to switch to BayerRG8 if the device supports it.
  s = GXSetEnumValueByString(h, "PixelFormat", "BayerRG8");
  if (s != GX_STATUS_SUCCESS) {
    // Fall back to whatever Bayer format is available
    RCLCPP_WARN(this->get_logger(),
      "BayerRG8 unavailable, leaving PixelFormat at its current value.");
  } else {
    pixel_format_bayer_ = GX_COLOR_FILTER_BAYER_RG;
  }

  // Acquisition buffer count (for queue depth)
  int64_t buf_num = 5;
  this->get_parameter("acquisition_buffer_number", buf_num);
  s = GXSetAcqusitionBufferNumber(h, static_cast<uint64_t>(buf_num));
  log_gx_error(this->get_logger(), s, "GXSetAcqusitionBufferNumber");

  // Get the data stream handle and payload size.
  GX_DS_HANDLE ds = nullptr;
  s = GXGetDataStreamHandleFromDev(h, 1, &ds);
  if (s != GX_STATUS_SUCCESS || ds == nullptr) {
    log_gx_error(this->get_logger(), s, "GXGetDataStreamHandleFromDev");
    return false;
  }
  ds_handle_ = ds;
  uint32_t payload = 0;
  s = GXGetPayLoadSize(ds, &payload);
  log_gx_error(this->get_logger(), s, "GXGetPayLoadSize");
  RCLCPP_INFO(this->get_logger(), "  Payload: %u bytes", payload);

  // Start streaming.
  s = GXStreamOn(h);
  if (s != GX_STATUS_SUCCESS) {
    log_gx_error(this->get_logger(), s, "GXStreamOn");
    return false;
  }

  return true;
}

void DahengCameraNode::close_device()
{
  if (device_handle_) {
    GX_DEV_HANDLE h = static_cast<GX_DEV_HANDLE>(device_handle_);
    GXStreamOff(h);
    GXCloseDevice(h);
    device_handle_ = nullptr;
    ds_handle_ = nullptr;
  }
}

void DahengCameraNode::capture_loop()
{
  RCLCPP_INFO(this->get_logger(), "[Daheng] capture thread started");

  std::vector<uint8_t> rgb_buf;

  while (rclcpp::ok() && running_.load()) {
    if (!device_handle_) {
      // Reopen every 1 second.
      std::this_thread::sleep_for(std::chrono::seconds(1));
      if (open_first_device()) {
        apply_parameters();
        rgb_buf.clear();
      }
      continue;
    }

    GX_DEV_HANDLE h = static_cast<GX_DEV_HANDLE>(device_handle_);
    PGX_FRAME_DATA_EX pFrame = nullptr;

    GX_STATUS s = GXDQBufEx(h, &pFrame, 1000);
    if (s == GX_STATUS_TIMEOUT) {
      continue;
    }
    if (s != GX_STATUS_SUCCESS) {
      log_gx_error(this->get_logger(), s, "GXDQBufEx");
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      continue;
    }
    if (pFrame == nullptr) {
      continue;
    }

    if (pFrame->nStatus != GX_FRAME_STATUS_SUCCESS) {
      RCLCPP_WARN(this->get_logger(),
        "Abnormal frame: status=%d, dropping", pFrame->nStatus);
      GXQBufEx(h, pFrame);
      continue;
    }

    const int width = pFrame->nWidth;
    const int height = pFrame->nHeight;
    const size_t rgb_size = static_cast<size_t>(width) * height * 3;
    if (rgb_buf.size() != rgb_size) {
      rgb_buf.resize(rgb_size);
    }

    // DxRaw8toRGB24 signature:
    //   DxRaw8toRGB24(pInput, pOutput, nWidth, nHeight, cvtype, nBayerType, bFlip)
    // The bayer pattern is configured at open_first_device via
    // PixelFormat=BayerRG8. bFlip=false to keep orientation raw.
    VxInt32 rc = DxRaw8toRGB24(
      reinterpret_cast<void *>(pFrame->pImgBuf),
      rgb_buf.data(),
      static_cast<VxUint32>(width),
      static_cast<VxUint32>(height),
      RAW2RGB_NEIGHBOUR,
      static_cast<DX_PIXEL_COLOR_FILTER>(pixel_format_bayer_),
      false);
    if (rc != DX_OK) {
      RCLCPP_WARN(this->get_logger(),
        "DxRaw8toRGB24 failed: rc=%d", static_cast<int>(rc));
      GXQBufEx(h, pFrame);
      continue;
    }

    // Build sensor_msgs/Image from the BGR buffer.
    image_msg_.header.stamp = this->now();
    image_msg_.header.frame_id = "camera_optical_frame";
    image_msg_.width = width;
    image_msg_.height = height;
    image_msg_.step = width * 3;
    image_msg_.encoding = "bgr8";
    if (image_msg_.data.size() != rgb_size) {
      image_msg_.data.resize(rgb_size);
    }
    std::memcpy(image_msg_.data.data(), rgb_buf.data(), rgb_size);

    camera_info_msg_.header = image_msg_.header;
    camera_pub_.publish(image_msg_, camera_info_msg_);

    GXQBufEx(h, pFrame);
  }

  RCLCPP_INFO(this->get_logger(), "[Daheng] capture thread exiting");
}

rcl_interfaces::msg::SetParametersResult DahengCameraNode::on_parameter_change(
  const std::vector<rclcpp::Parameter> & parameters)
{
  rcl_interfaces::msg::SetParametersResult result;
  result.successful = true;

  bool need_apply = false;
  for (const auto & param : parameters) {
    const std::string & name = param.get_name();
    if (
      name == "exposure_time" || name == "gain" ||
      name == "roi_width" || name == "roi_height" ||
      name == "frame_rate" || name == "bayer_pattern" ||
      name == "acquisition_buffer_number")
    {
      need_apply = true;
    } else {
      result.successful = false;
      result.reason = "Unknown parameter: " + name;
    }
  }

  if (need_apply && result.successful) {
    apply_parameters();
  }
  return result;
}

}  // namespace daheng_camera

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(daheng_camera::DahengCameraNode)
