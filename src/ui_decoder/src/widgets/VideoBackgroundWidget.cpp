#include "VideoBackgroundWidget.h"

#include <QDateTime>
#include <QPainter>

namespace ui_decoder {

VideoBackgroundWidget::VideoBackgroundWidget(QWidget *parent)
    : QWidget(parent), m_rotate180(false), m_fps(0.0),
      m_signalLostTimeoutMs(1500), m_lastFrameMs(0) {
  setAttribute(Qt::WA_OpaquePaintEvent);
  setAttribute(Qt::WA_NoSystemBackground);
  setAutoFillBackground(true);

  QPalette pal = palette();
  pal.setColor(QPalette::Window, Qt::black);
  setPalette(pal);
}

void VideoBackgroundWidget::setRotate180(bool rotate) {
  QMutexLocker locker(&m_mutex);
  if (m_rotate180 == rotate)
    return;
  m_rotate180 = rotate;
  m_scaledTargetSize = QSize();
  m_scaledFrame = QImage();
  locker.unlock();
  update();
}

void VideoBackgroundWidget::addOverlay(QWidget *w, int dx, int dy) {
  if (!w)
    return;
  w->setParent(this);
  m_overlays.append({w, dx, dy});
  repositionOverlays();
  w->show();
  w->raise();
}

void VideoBackgroundWidget::setFullScreenOverlay(QWidget *w) {
  if (!w)
    return;
  w->setParent(this);
  m_fullScreenOverlay = w;
  w->setGeometry(rect());
  w->show();
  w->raise();
}

void VideoBackgroundWidget::repositionOverlays() {
  const QPoint center(width() / 2, height() / 2);
  for (const OverlayEntry &entry : m_overlays) {
    if (!entry.widget)
      continue;
    const QSize sz = entry.widget->sizeHint().isEmpty()
                         ? entry.widget->size()
                         : entry.widget->size();
    entry.widget->move(center.x() - sz.width() / 2 + entry.dx,
                       center.y() - sz.height() / 2 + entry.dy);
  }
}

void VideoBackgroundWidget::setFrame(const QImage &frame) {
  {
    QMutexLocker locker(&m_mutex);
    m_frame = frame;
    m_scaledTargetSize = QSize();
    m_scaledFrame = QImage();
    m_lastFrameMs = QDateTime::currentMSecsSinceEpoch();
  }
  update();
}

void VideoBackgroundWidget::setFps(double fps) {
  QMutexLocker locker(&m_mutex);
  m_fps = fps;
}

void VideoBackgroundWidget::setSignalLostTimeoutMs(int timeoutMs) {
  QMutexLocker locker(&m_mutex);
  m_signalLostTimeoutMs = timeoutMs;
}

QImage VideoBackgroundWidget::currentFrame() const {
  QMutexLocker locker(&m_mutex);
  return m_frame;
}

QImage VideoBackgroundWidget::scaledFrameLocked() const {
  if (m_frame.isNull() || size().isEmpty()) {
    return QImage();
  }
  if (!m_scaledFrame.isNull() && m_scaledTargetSize == size()) {
    return m_scaledFrame;
  }

  const QImage forDisplay =
      m_rotate180 ? m_frame.mirrored(true, true) : m_frame;
  m_scaledFrame = forDisplay.scaled(size(), Qt::KeepAspectRatio,
                                    Qt::SmoothTransformation);
  m_scaledTargetSize = size();
  return m_scaledFrame;
}

void VideoBackgroundWidget::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  {
    QMutexLocker locker(&m_mutex);
    m_scaledTargetSize = QSize();
    m_scaledFrame = QImage();
  }
  repositionOverlays();
  if (m_fullScreenOverlay) {
    m_fullScreenOverlay->setGeometry(rect());
    m_fullScreenOverlay->raise();
  }
}

void VideoBackgroundWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter painter(this);

  QImage scaled;
  double fps = 0.0;
  int signalLostTimeout = 1500;
  qint64 lastFrameMs = 0;
  {
    QMutexLocker locker(&m_mutex);
    scaled = scaledFrameLocked();
    fps = m_fps;
    signalLostTimeout = m_signalLostTimeoutMs;
    lastFrameMs = m_lastFrameMs;
  }

  if (!scaled.isNull()) {
    const int x = (width() - scaled.width()) / 2;
    const int y = (height() - scaled.height()) / 2;
    painter.fillRect(rect(), Qt::black);
    painter.drawImage(x, y, scaled);
  } else {
    painter.fillRect(rect(), Qt::black);
  }

  if (fps > 0.0) {
    painter.setPen(Qt::white);
    QFont font("Monospace", 12, QFont::Bold);
    painter.setFont(font);
    painter.fillRect(QRect(4, 4, 120, 24), QColor(0, 0, 0, 120));
    painter.drawText(QRect(8, 4, 116, 24), Qt::AlignVCenter | Qt::AlignLeft,
                     QString("FPS: %1").arg(fps, 0, 'f', 1));
  }

  const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
  if (lastFrameMs > 0 && (nowMs - lastFrameMs) > signalLostTimeout) {
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 24, QFont::Bold));
    painter.drawText(rect(), Qt::AlignCenter, "SIGNAL LOST");
  }
}

} // namespace ui_decoder
