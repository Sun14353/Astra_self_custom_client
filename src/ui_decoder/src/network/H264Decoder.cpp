#include "H264Decoder.h"
#include "H264AnnexB.h"

#include <QDebug>

#include <cstring>

namespace ui_decoder {

namespace {

bool verboseVideoLogEnabled() {
  static const bool enabled = qEnvironmentVariableIsSet("RM_VERBOSE_VIDEO_LOG");
  return enabled;
}

} // namespace

H264Decoder::H264Decoder(QObject *parent) : QObject(parent) {}

H264Decoder::~H264Decoder() { cleanup(); }

bool H264Decoder::initialize() {
  QMutexLocker locker(&m_mutex);
  if (m_initialized)
    return true;

  m_codec = avcodec_find_decoder(AV_CODEC_ID_H264);
  if (!m_codec) {
    emit decodingError("[ui_decoder] H264 decoder not found");
    return false;
  }

  m_parser = av_parser_init(AV_CODEC_ID_H264);
  if (!m_parser) {
    emit decodingError("[ui_decoder] Failed to init H264 parser");
    return false;
  }

  m_codecCtx = avcodec_alloc_context3(m_codec);
  if (!m_codecCtx) {
    emit decodingError("[ui_decoder] Failed to alloc codec context");
    return false;
  }

  m_codecCtx->flags |= AV_CODEC_FLAG_LOW_DELAY;
  m_codecCtx->flags2 |= AV_CODEC_FLAG2_FAST;
  m_codecCtx->skip_loop_filter = AVDISCARD_NONREF;

  if (avcodec_open2(m_codecCtx, m_codec, nullptr) < 0) {
    emit decodingError("[ui_decoder] Failed to open H264 codec");
    return false;
  }

  m_frame = av_frame_alloc();
  m_packet = av_packet_alloc();
  m_swsCtx = nullptr;
  m_rgbBuffer = nullptr;
  m_initialized = true;
  m_decodedFrameCount = 0;
  clearCachedState();
  // 初始即进入“等待关键帧”状态：订阅建立时刻可能落在码流中间，
  // 此时尚未收到 SPS/PPS，直接解码会产生大量 “non-existing PPS” 报错。
  // 只有收到首个 IDR 关键帧后才真正开始输出画面。
  m_needKeyframe = true;
  return true;
}

QImage H264Decoder::decode(const QByteArray &data) {
  // 与 doorlock_decoder 的处理链一致：先用 av_parser_parse2 将 150 字节
  // 分片拼成完整访问单元，再交给 avcodec_send_packet 解码。
  QMutexLocker locker(&m_mutex);
  if (!m_initialized || !m_parser || !m_codecCtx || !m_packet || !m_frame)
    return QImage();

  m_inputBuffer.append(data);
  constexpr int kMaxInputBufferSize = 1 * 1024 * 1024;
  if (m_inputBuffer.size() > kMaxInputBufferSize) {
    qWarning() << "[ui_decoder] H264Decoder: input buffer exceeded"
               << kMaxInputBufferSize << "bytes, resetting to recover sync";
    m_inputBuffer.clear();
    avcodec_flush_buffers(m_codecCtx);
  }

  if (verboseVideoLogEnabled()) {
    qDebug() << "[ui_decoder] H264Decoder: Received data size=" << data.size()
             << " buffer_size=" << m_inputBuffer.size()
             << " prefix(hex)=" << m_inputBuffer.left(16).toHex();
  }
  QImage outImage;

  const uint8_t *buffer =
      reinterpret_cast<const uint8_t *>(m_inputBuffer.constData());
  int buf_size = m_inputBuffer.size();
  int consumed = 0;

  const uint8_t *input = buffer;
  int input_size = buf_size;

  while (input_size > 0) {
    uint8_t *out_data = nullptr;
    int out_size = 0;
    int len =
        av_parser_parse2(m_parser, m_codecCtx, &out_data, &out_size, input,
                         input_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
    if (len <= 0)
      break;
    input += len;
    input_size -= len;
    consumed += len;

    if (out_size > 0) {
      const QByteArray sps = h264AnnexBExtractNalUnit(out_data, out_size, 7);
      const QByteArray pps = h264AnnexBExtractNalUnit(out_data, out_size, 8);
      if (!sps.isEmpty()) {
        m_sps = sps;
        m_seenSPS = true;
      }
      if (!pps.isEmpty()) {
        m_pps = pps;
        m_seenPPS = true;
      }
      const bool containsIdr = h264AnnexBContainsNalType(out_data, out_size, 5);
      const bool containsNonIdrVcl =
          h264AnnexBContainsNalType(out_data, out_size, 1) ||
          h264AnnexBContainsNalType(out_data, out_size, 2) ||
          h264AnnexBContainsNalType(out_data, out_size, 3) ||
          h264AnnexBContainsNalType(out_data, out_size, 4);

      if (m_needKeyframe) {
        if (!containsIdr && containsNonIdrVcl) {
          continue;
        }
        if (containsIdr) {
          m_needKeyframe = false;
          qWarning() << "[ui_decoder] H264Decoder: 收到 IDR 关键帧，恢复解码";
        }
      }

      av_packet_unref(m_packet);
      if (av_new_packet(m_packet, out_size) < 0)
        continue;
      memcpy(m_packet->data, out_data, out_size);

      int ret = avcodec_send_packet(m_codecCtx, m_packet);
      if (ret < 0) {
        char errbuf[128] = {0};
        av_strerror(ret, errbuf, sizeof(errbuf));
        qWarning() << "[ui_decoder] H264Decoder: avcodec_send_packet failed ret="
                   << ret << "err=" << errbuf;
        av_packet_unref(m_packet);
        continue;
      }

      while (ret >= 0) {
        ret = avcodec_receive_frame(m_codecCtx, m_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
          break;
        if (ret < 0)
          break;

        outImage = convertToQImage(m_frame);
        m_decodedFrameCount++;
        emit frameDecoded(outImage);
        av_frame_unref(m_frame);
      }
      av_packet_unref(m_packet);
    }
  }

  if (consumed > 0) {
    m_inputBuffer.remove(0, consumed);
  }

  return outImage;
}

void H264Decoder::flush() {
  QMutexLocker locker(&m_mutex);
  if (!m_initialized || !m_codecCtx || !m_frame)
    return;
  avcodec_send_packet(m_codecCtx, nullptr);
  while (avcodec_receive_frame(m_codecCtx, m_frame) == 0) {
    QImage img = convertToQImage(m_frame);
    emit frameDecoded(img);
    av_frame_unref(m_frame);
  }
}

QImage H264Decoder::convertToQImage(AVFrame *frame) {
  if (!frame)
    return QImage();
  updateSwsContext(frame->width, frame->height);

  int rgbLinesize = 3 * frame->width;
  rgbLinesize = (rgbLinesize + 3) & ~3;
  int bufSize = av_image_get_buffer_size(AV_PIX_FMT_BGR24, frame->width,
                                          frame->height, 1);
  if (bufSize <= 0 || rgbLinesize * frame->height > bufSize) {
    bufSize = rgbLinesize * frame->height;
  }
  if (!m_rgbBuffer || m_rgbBufferSize < bufSize) {
    av_free(m_rgbBuffer);
    m_rgbBuffer = static_cast<uint8_t *>(av_malloc(bufSize));
    if (!m_rgbBuffer) {
      m_rgbBufferSize = 0;
      return QImage();
    }
    m_rgbBufferSize = bufSize;
  }

  uint8_t *dstData[1] = {m_rgbBuffer};
  int dstLinesize[1] = {rgbLinesize};

  sws_scale(m_swsCtx, frame->data, frame->linesize, 0, frame->height, dstData,
            dstLinesize);

  QImage img(m_rgbBuffer, frame->width, frame->height, rgbLinesize,
             QImage::Format_BGR888);
  // 解码层只负责像素格式转换，UI 叠加不写入帧数据。
  return img.copy();
}

void H264Decoder::updateSwsContext(int width, int height) {
  if (m_width == width && m_height == height && m_swsCtx)
    return;
  if (m_swsCtx) {
    sws_freeContext(m_swsCtx);
    m_swsCtx = nullptr;
  }
  m_width = width;
  m_height = height;
  m_swsCtx = sws_getContext(
      width, height, static_cast<AVPixelFormat>(m_frame->format), width, height,
      AV_PIX_FMT_BGR24, SWS_BILINEAR, nullptr, nullptr, nullptr);
}

void H264Decoder::cleanup() {
  if (m_swsCtx)
    sws_freeContext(m_swsCtx);
  if (m_frame)
    av_frame_free(&m_frame);
  if (m_packet)
    av_packet_free(&m_packet);
  if (m_parser)
    av_parser_close(m_parser);
  if (m_codecCtx)
    avcodec_free_context(&m_codecCtx);
  if (m_rgbBuffer) {
    av_free(m_rgbBuffer);
    m_rgbBuffer = nullptr;
  }
  m_rgbBufferSize = 0;
  m_initialized = false;
}

void H264Decoder::clearCachedState() {
  m_inputBuffer.clear();
  m_sps.clear();
  m_pps.clear();
  m_seenSPS = false;
  m_seenPPS = false;

  if (m_packet) {
    av_packet_unref(m_packet);
  }
  if (m_frame) {
    av_frame_unref(m_frame);
  }
}

void H264Decoder::resetParserAndCodec() {
  QMutexLocker locker(&m_mutex);
  qWarning() << "[ui_decoder] H264Decoder: resetParserAndCodec() called";
  const QByteArray cachedSps = m_sps;
  const QByteArray cachedPps = m_pps;
  clearCachedState();
  if (m_parser) {
    av_parser_close(m_parser);
    m_parser = nullptr;
  }

  if (m_codecCtx) {
    avcodec_free_context(&m_codecCtx);
    m_codecCtx = nullptr;
  }
  m_initialized = false;

  const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
  if (codec) {
    m_parser = av_parser_init(AV_CODEC_ID_H264);
    m_codecCtx = avcodec_alloc_context3(codec);
    if (!m_parser || !m_codecCtx) {
      emit decodingError(
          "[ui_decoder] H264Decoder: failed to reinitialize parser/context");
      if (m_parser) {
        av_parser_close(m_parser);
        m_parser = nullptr;
      }
      if (m_codecCtx) {
        avcodec_free_context(&m_codecCtx);
        m_codecCtx = nullptr;
      }
    } else {
      m_codecCtx->flags |= AV_CODEC_FLAG_LOW_DELAY;
      m_codecCtx->flags2 |= AV_CODEC_FLAG2_FAST;
      m_codecCtx->skip_loop_filter = AVDISCARD_NONREF;
      if (avcodec_open2(m_codecCtx, codec, nullptr) < 0) {
        emit decodingError(
            "[ui_decoder] H264Decoder: failed to open codec during reset");
        av_parser_close(m_parser);
        avcodec_free_context(&m_codecCtx);
        m_parser = nullptr;
        m_codecCtx = nullptr;
      } else {
        m_codec = codec;
        m_initialized = true;
        m_sps = cachedSps;
        m_pps = cachedPps;
        m_seenSPS = !m_sps.isEmpty();
        m_seenPPS = !m_pps.isEmpty();

        const QByteArray cachedParameterSets = cachedSps + cachedPps;
        if (!cachedParameterSets.isEmpty()) {
          AVPacket *packet = av_packet_alloc();
          if (packet) {
            if (av_new_packet(packet, cachedParameterSets.size()) >= 0) {
              memcpy(packet->data, cachedParameterSets.constData(),
                     cachedParameterSets.size());
              const int ret = avcodec_send_packet(m_codecCtx, packet);
              if (ret < 0) {
                char errbuf[128] = {0};
                av_strerror(ret, errbuf, sizeof(errbuf));
                qWarning()
                    << "[ui_decoder] H264Decoder: cached SPS/PPS restore "
                       "failed ret="
                    << ret << "err=" << errbuf;
              }
            }
            av_packet_free(&packet);
          }
        }
      }
    }
  } else {
    emit decodingError("[ui_decoder] H264Decoder: codec not found during reset");
  }

  m_inputBuffer.clear();
  m_decodedFrameCount = 0;
  m_needKeyframe = true;
  if (m_rgbBuffer) {
    av_free(m_rgbBuffer);
    m_rgbBuffer = nullptr;
  }
  m_rgbBufferSize = 0;
}

} // namespace ui_decoder
