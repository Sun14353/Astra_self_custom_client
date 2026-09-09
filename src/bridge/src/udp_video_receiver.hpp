#pragma once

// ============================================================
// udp_video_receiver.hpp
//
// 自定义客户端通过 UDP 监听 3334 端口获取图传码流
//   - 编码格式: HEVC (H.265)
//   - 每个 UDP 包前 8 字节固定包头:
//       帧编号(递增)            : 2 byte
//       当前帧内分片序号         : 2 byte
//       当前帧总字节数           : 4 byte
//   - 剩余部分是 HEVC 码流的分片
//
// 使用方式:
//   UdpVideoReceiver rec(3334);
//   rec.SetFrameCallback([](const HevcFrame& f){ ... });
//   rec.Start();        // 启动后台接收线程
//   ...
//   rec.Stop();
// ============================================================

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#pragma pack(push, 1)
struct UdpVideoHeader {
    uint16_t frame_id;        // 帧编号(递增)
    uint16_t fragment_index;  // 当前帧内分片序号
    uint32_t frame_total_bytes; // 当前帧总字节数
};
static_assert(sizeof(UdpVideoHeader) == 8, "header must be 8 bytes");
#pragma pack(pop)

// 一帧完整的 HEVC 数据(已重组好)
struct HevcFrame {
    uint32_t              frame_id = 0;
    std::vector<uint8_t>  data;     // 该帧完整的 HEVC 码流
};

class UdpVideoReceiver {
public:
    using FrameCallback = std::function<void(const HevcFrame&)>;

    explicit UdpVideoReceiver(uint16_t port = 3334);
    ~UdpVideoReceiver();

    UdpVideoReceiver(const UdpVideoReceiver&)            = delete;
    UdpVideoReceiver& operator=(const UdpVideoReceiver&) = delete;

    void SetFrameCallback(FrameCallback cb);
    void Start();
    void Stop();

    uint16_t Port() const { return m_port; }
    bool Running() const { return m_running.load(); }

private:
    void receive_loop();

    uint16_t                       m_port;
    int                            m_sock = -1;
    std::atomic<bool>              m_running{false};
    std::thread                    m_thread;
    FrameCallback                  m_cb;

    // 帧重组缓冲
    std::mutex                     m_bufMutex;
    uint32_t                       m_curFrameId   = 0;
    uint32_t                       m_curFrameSize = 0;
    uint32_t                       m_curFragIndex = 0;  // 已收到的最大分片序号
    std::vector<uint8_t>           m_curFrameData;
};
