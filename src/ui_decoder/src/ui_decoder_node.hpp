/**
 * @file ui_decoder_node.hpp
 * @brief ui_decoder ROS 2 节点：订阅 VideoPacket 码流，解码并显示 UI。
 */

#ifndef UIDECODER_NODE_HPP
#define UIDECODER_NODE_HPP

#include <rclcpp/rclcpp.hpp>
#include <doorlock_sniper/msg/video_packet.hpp>

#include <chrono>
#include <cstdint>
#include <string>

class QImage;
class QString;
class QTimer;
class QWidget;

namespace ui_decoder {
class H264Decoder;
class VideoBackgroundWidget;
class CrosshairWidget;
class HeatRingWidget;
} // namespace ui_decoder

class UiDecoderNode : public rclcpp::Node {
public:
  explicit UiDecoderNode(const rclcpp::NodeOptions &options = rclcpp::NodeOptions());
  ~UiDecoderNode() override;

  void show();
  QWidget *window() const { return m_window; }

private:
  void configureFromParams();
  void onPacket(const doorlock_sniper::msg::VideoPacket::SharedPtr msg);
  void onDecodingError(const QString &error);

  rclcpp::Subscription<doorlock_sniper::msg::VideoPacket>::SharedPtr m_sub;

  ui_decoder::H264Decoder *m_decoder = nullptr;
  ui_decoder::VideoBackgroundWidget *m_video = nullptr;
  ui_decoder::CrosshairWidget *m_crosshair = nullptr;
  ui_decoder::HeatRingWidget *m_heatRing = nullptr;
  QWidget *m_window = nullptr;

  QTimer *m_fpsTimer = nullptr;
  QTimer *m_heatTimer = nullptr;

  // 码流/帧统计
  bool m_haveLastSeq = false;
  uint64_t m_lastSeq = 0;
  uint64_t m_packetCount = 0;
  uint64_t m_gapCount = 0;
  uint64_t m_frameCount = 0;
  uint64_t m_fpsWindowFrames = 0;
  std::chrono::steady_clock::time_point m_fpsWindowStart;

  // 演示热量源（无赛场数据时驱动热量环动画）
  int m_heatCurrent = 0;
  int m_heatMax = 240;
  int m_heatStep = 1;
  bool m_demoHeat = false;
};

#endif // UIDECODER_NODE_HPP
