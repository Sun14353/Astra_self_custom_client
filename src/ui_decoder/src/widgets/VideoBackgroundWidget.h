/**
 * @file VideoBackgroundWidget.h
 * @brief 视频背景显示控件（从 EGA_Client 简化迁移）
 *
 * 负责绘制解码后的视频帧（保持宽高比、居中），并在画面上叠加 FPS 与
 * 信号丢失提示。准星与热量环作为独立子控件叠加在其上。
 */

#ifndef UIDECODER_VIDEOBACKGROUNDWIDGET_H
#define UIDECODER_VIDEOBACKGROUNDWIDGET_H

#include <QImage>
#include <QMutex>
#include <QWidget>

namespace ui_decoder {

class VideoBackgroundWidget : public QWidget {
  Q_OBJECT

public:
  explicit VideoBackgroundWidget(QWidget *parent = nullptr);

  void setRotate180(bool rotate);

  /**
   * @brief 将子控件作为叠加层，居中(可带偏移)叠放到视频画面上。
   * @param w  子控件（父对象将改为本控件）
   * @param dx 相对中心 x 偏移
   * @param dy 相对中心 y 偏移
   */
  void addOverlay(QWidget *w, int dx = 0, int dy = 0);

  /**
   * @brief 将一个子控件作为铺满整个视频画面的叠加层（例如 QML HUD）。
   * @param w 子控件（父对象将改为本控件，并随本控件 resize 自动铺满）
   */
  void setFullScreenOverlay(QWidget *w);

public slots:
  void setFrame(const QImage &frame);
  void setFps(double fps);
  void setSignalLostTimeoutMs(int timeoutMs);

protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  struct OverlayEntry {
    QWidget *widget;
    int dx;
    int dy;
  };

  void repositionOverlays();
  QImage currentFrame() const;
  QImage scaledFrameLocked() const;

  mutable QMutex m_mutex;
  QImage m_frame;
  mutable QImage m_scaledFrame;
  mutable QSize m_scaledTargetSize;
  bool m_rotate180;
  double m_fps;
  int m_signalLostTimeoutMs;
  qint64 m_lastFrameMs;
  QList<OverlayEntry> m_overlays;
  QWidget *m_fullScreenOverlay = nullptr;
};

} // namespace ui_decoder

#endif // UIDECODER_VIDEOBACKGROUNDWIDGET_H
