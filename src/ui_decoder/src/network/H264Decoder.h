/**
 * @file H264Decoder.h
 * @brief H.264 连续流解码器（基于 FFmpeg）
 *
 * 从 EGA_Client/RM26_Client/src/network/H264Decoder.h 迁移，命名空间改为
 * ui_decoder，去除 RM_ENABLE_FFMPEG 条件编译（本包强制依赖 FFmpeg）。
 */

#ifndef UIDECODER_H264DECODER_H
#define UIDECODER_H264DECODER_H

#include <QByteArray>
#include <QImage>
#include <QMutex>
#include <QObject>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace ui_decoder {

class H264Decoder : public QObject {
  Q_OBJECT
public:
  explicit H264Decoder(QObject *parent = nullptr);
  ~H264Decoder();

  bool initialize();
  QImage decode(const QByteArray &data);
  void flush();
  void resetParserAndCodec();

  bool isInitialized() const { return m_initialized; }
  int getDecodedFrameCount() const { return m_decodedFrameCount; }

signals:
  void frameDecoded(const QImage &image);
  void decodingError(const QString &error);

private:
  void clearCachedState();
  const AVCodec *m_codec = nullptr;
  AVCodecContext *m_codecCtx = nullptr;
  AVCodecParserContext *m_parser = nullptr;
  AVFrame *m_frame = nullptr;
  AVPacket *m_packet = nullptr;
  SwsContext *m_swsCtx = nullptr;
  uint8_t *m_rgbBuffer = nullptr;
  int m_rgbBufferSize = 0;
  int m_width = 0;
  int m_height = 0;

  bool m_initialized = false;
  bool m_needKeyframe = false; // 重置后保持为真，直到收到下一帧 IDR。
  int m_decodedFrameCount = 0;
  QMutex m_mutex;
  QByteArray m_inputBuffer; // 累积收到的 H.264 分片。
  QByteArray m_sps;
  QByteArray m_pps;
  bool m_seenSPS = false;
  bool m_seenPPS = false;

  QImage convertToQImage(AVFrame *frame);
  void updateSwsContext(int width, int height);
  void cleanup();
};

} // namespace ui_decoder

#endif // UIDECODER_H264DECODER_H
