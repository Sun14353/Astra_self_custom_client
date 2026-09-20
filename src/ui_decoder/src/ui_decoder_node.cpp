#include "ui_decoder_node.hpp"

#include "network/H264Decoder.h"
#include "widgets/CrosshairWidget.h"
#include "widgets/HeatRingWidget.h"
#include "widgets/VideoBackgroundWidget.h"

#include <QApplication>
#include <QColor>
#include <QImage>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace {

int clampInt(int v, int lo, int hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

} // namespace

UiDecoderNode::UiDecoderNode(const rclcpp::NodeOptions &options)
    : Node("ui_decoder_node", options) {
  // 顶层窗口
  m_window = new QWidget();
  m_window->setWindowTitle("ui_decoder - H.264 解码与瞄准 UI");

  m_video = new ui_decoder::VideoBackgroundWidget(m_window);
  auto *layout = new QVBoxLayout(m_window);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(m_video);
  m_window->setLayout(layout);

  // 叠加层：准星 + 热量环（居中叠放）
  m_crosshair = new ui_decoder::CrosshairWidget(m_video);
  m_heatRing = new ui_decoder::HeatRingWidget(m_video);
  m_video->addOverlay(m_heatRing, 0, 0);
  m_video->addOverlay(m_crosshair, 0, 0);

  // 解码器（父对象为窗口，随窗口析构）
  m_decoder = new ui_decoder::H264Decoder(m_window);
  if (!m_decoder->initialize()) {
    RCLCPP_ERROR(this->get_logger(), "H264 decoder initialize failed");
  }
  QObject::connect(m_decoder, &ui_decoder::H264Decoder::frameDecoded, m_video,
                   [this](const QImage &img) {
                     m_frameCount++;
                     m_fpsWindowFrames++;
                     m_video->setFrame(img);
                   });
  QObject::connect(m_decoder, &ui_decoder::H264Decoder::decodingError, m_video,
                   [this](const QString &error) {
                     RCLCPP_ERROR(this->get_logger(), "%s",
                                  error.toStdString().c_str());
                   });

  configureFromParams();

  // FPS 统计：每秒刷新一次
  m_fpsTimer = new QTimer(m_window);
  m_fpsTimer->setInterval(1000);
  m_fpsWindowStart = std::chrono::steady_clock::now();
  QObject::connect(m_fpsTimer, &QTimer::timeout, m_video, [this]() {
    const auto now = std::chrono::steady_clock::now();
    const double elapsed =
        std::chrono::duration_cast<std::chrono::duration<double>>(
            now - m_fpsWindowStart)
            .count();
    if (elapsed > 0.0) {
      const double fps = m_fpsWindowFrames / elapsed;
      m_video->setFps(fps);
    }
    m_fpsWindowFrames = 0;
    m_fpsWindowStart = now;
  });
  m_fpsTimer->start();

  // 演示热量源
  if (m_demoHeat) {
    m_heatTimer = new QTimer(m_window);
    m_heatTimer->setInterval(60);
    QObject::connect(m_heatTimer, &QTimer::timeout, m_video, [this]() {
      m_heatCurrent += m_heatStep;
      if (m_heatCurrent >= m_heatMax) {
        m_heatCurrent = m_heatMax;
        m_heatStep = -1;
      } else if (m_heatCurrent <= 0) {
        m_heatCurrent = 0;
        m_heatStep = 1;
      }
      m_heatRing->setHeat(m_heatCurrent, m_heatMax);
    });
    m_heatTimer->start();
  }

  // 订阅视频码流（与发布端一致：reliable + keep-last 3000）
  const std::string topic = this->declare_parameter("topic", "video_stream");
  rclcpp::QoS qos(rclcpp::KeepLast(3000));
  qos.reliability(rclcpp::ReliabilityPolicy::Reliable);
  qos.durability(rclcpp::DurabilityPolicy::Volatile);

  m_sub = this->create_subscription<doorlock_sniper::msg::VideoPacket>(
      topic, qos,
      [this](const doorlock_sniper::msg::VideoPacket::SharedPtr msg) {
        onPacket(msg);
      });

  RCLCPP_INFO(this->get_logger(),
              "ui_decoder_node ready, subscribing topic=%s", topic.c_str());
}

UiDecoderNode::~UiDecoderNode() {
  // QWidget/QObject 子树由 Qt 自动回收；ROS 2 订阅由基类析构处理。
}

void UiDecoderNode::show() {
  m_window->show();
}

void UiDecoderNode::configureFromParams() {
  // 准星
  const std::string type =
      this->declare_parameter("crosshair_type", "sniper");
  ui_decoder::CrosshairWidget::CrosshairType ct =
      ui_decoder::CrosshairWidget::Sniper;
  if (type == "default") {
    ct = ui_decoder::CrosshairWidget::Default;
  } else if (type == "shotgun") {
    ct = ui_decoder::CrosshairWidget::Shotgun;
  } else if (type == "sniper") {
    ct = ui_decoder::CrosshairWidget::Sniper;
  }
  m_crosshair->setCrosshairType(ct);

  const int size = this->declare_parameter("crosshair_size", 40);
  const int thickness = this->declare_parameter("crosshair_thickness", 2);
  m_crosshair->setSize(size);
  m_crosshair->setThickness(thickness);

  const std::string colorName =
      this->declare_parameter("crosshair_color", "#00ff00");
  m_crosshair->setColor(QColor(QString::fromStdString(colorName)));

  m_crosshair->setGlowEnabled(this->declare_parameter("glow_enabled", true));
  m_crosshair->setPulseEnabled(this->declare_parameter("pulse_enabled", false));
  m_crosshair->setHitMarkerEnabled(
      this->declare_parameter("hitmarker_enabled", true));
  m_crosshair->setDynamicSize(
      this->declare_parameter("dynamic_size", false));

  // 热量环
  m_heatMax = clampInt(this->declare_parameter("heat_max", 240), 1, 10000);
  m_heatCurrent =
      clampInt(this->declare_parameter("heat_current", 60), 0, m_heatMax);
  m_demoHeat = this->declare_parameter("demo_heat", true);
  m_heatRing->setHeat(m_heatCurrent, m_heatMax);
  m_heatRing->setGlowEnabled(this->declare_parameter("heat_glow", true));
  m_heatRing->setParticleEnabled(
      this->declare_parameter("heat_particles", true));

  // 视频
  const bool rotate180 = this->declare_parameter("rotate_180", false);
  m_video->setRotate180(rotate180);

  // 窗口
  const int w = clampInt(this->declare_parameter("window_width", 800), 320, 4000);
  const int h = clampInt(this->declare_parameter("window_height", 800), 320, 4000);
  m_window->resize(w, h);
}

void UiDecoderNode::onPacket(
    const doorlock_sniper::msg::VideoPacket::SharedPtr msg) {
  m_packetCount++;

  // 丢包/乱序检测：任意 150B 分片丢失都会破坏码流同步，
  // 直接重置解码器并等待下一组 SPS/PPS + IDR。
  if (m_haveLastSeq && msg->sequence_id != m_lastSeq + 1) {
    m_gapCount++;
    RCLCPP_WARN(this->get_logger(),
                "VideoPacket sequence gap: %lu -> %lu (gaps=%lu), reset decoder",
                static_cast<unsigned long>(m_lastSeq),
                static_cast<unsigned long>(msg->sequence_id),
                static_cast<unsigned long>(m_gapCount));
    m_decoder->resetParserAndCodec();
  }
  m_lastSeq = msg->sequence_id;
  m_haveLastSeq = true;

  const QByteArray chunk(reinterpret_cast<const char *>(msg->data.data()),
                         static_cast<int>(msg->data.size()));
  m_decoder->decode(chunk);

  if (m_packetCount % 600 == 0) {
    RCLCPP_INFO(this->get_logger(),
                "Rx packets=%lu decoded_frames=%lu gaps=%lu",
                static_cast<unsigned long>(m_packetCount),
                static_cast<unsigned long>(m_frameCount),
                static_cast<unsigned long>(m_gapCount));
  }
}

void UiDecoderNode::onDecodingError(const QString &error) {
  RCLCPP_ERROR(this->get_logger(), "%s", error.toStdString().c_str());
}
