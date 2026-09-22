/**
 * @file main.cpp
 * @brief ui_decoder 入口：Qt 事件循环 + rclcpp::spin_some 集成。
 *
 * 采用经典的 QTimer 驱动 spin_some 模式，让 Qt 事件循环与 ROS 2 回调
 * 在同一个线程内交替执行，避免多线程竞争；解码在回调中同步完成。
 */

#include "ui_decoder_node.hpp"

#include <QApplication>
#include <QSurfaceFormat>
#include <QTimer>

#include <memory>

int main(int argc, char **argv) {
  // rclcpp::init 会消费 ROS 相关参数，剩余参数再交给 QApplication。
  rclcpp::init(argc, argv);

  // 关键：QQuickWidget 透明叠加层需要 alpha 通道，必须在 QApplication
  // 创建之前设置默认 SurfaceFormat，否则上下文已初始化导致设置无效。
  QSurfaceFormat format;
  format.setAlphaBufferSize(8);
  QSurfaceFormat::setDefaultFormat(format);

  // 初始化 QML 资源（HUD 覆盖层）。
  Q_INIT_RESOURCE(qml);

  // QApplication 必须先于任何 QWidget 创建。
  QApplication app(argc, argv);

  auto node = std::make_shared<UiDecoderNode>();
  node->show();

  // 以 ~100Hz 驱动 spin_some，处理订阅回调与 Qt 事件。
  // 仅在 rclcpp::ok() 时执行，避免 shutdown 后 spin_some 抛 RCLError。
  QTimer spinTimer;
  QObject::connect(&spinTimer, &QTimer::timeout, [node]() {
    if (rclcpp::ok()) {
      rclcpp::spin_some(node);
    }
  });
  spinTimer.start(10);

  // SIGINT/SIGTERM 会触发 rclcpp 内部的 shutdown，这里随之退出 Qt 事件循环，
  // 保证 app.exec() 能正常返回而不是被信号处理器直接杀死。
  rclcpp::on_shutdown([&app]() { app.quit(); });

  const int ret = app.exec();

  if (rclcpp::ok()) {
    rclcpp::shutdown();
  }
  return ret;
}
