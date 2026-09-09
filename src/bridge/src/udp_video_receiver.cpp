// ============================================================
// udp_video_receiver.cpp
// ============================================================

#include "udp_video_receiver.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {
void log_info(const std::string& msg) {
    std::cerr << "[UdpVideoReceiver] " << msg << std::endl;
}
} // namespace

UdpVideoReceiver::UdpVideoReceiver(uint16_t port) : m_port(port) {}

UdpVideoReceiver::~UdpVideoReceiver() {
    Stop();
}

void UdpVideoReceiver::SetFrameCallback(FrameCallback cb) {
    m_cb = std::move(cb);
}

void UdpVideoReceiver::Start() {
    bool expected = false;
    if (!m_running.compare_exchange_strong(expected, true)) {
        return;  // 已经在跑
    }

    m_sock = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (m_sock < 0) {
        m_running.store(false);
        log_info(std::string("socket() failed: ") + std::strerror(errno));
        return;
    }

    // 允许端口复用
    int yes = 1;
    ::setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    // 接收缓冲区放大
    int buf_size = 4 * 1024 * 1024;
    ::setsockopt(m_sock, SOL_SOCKET, SO_RCVBUF, &buf_size, sizeof(buf_size));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(m_port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (::bind(m_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        log_info(std::string("bind failed: ") + std::strerror(errno));
        ::close(m_sock);
        m_sock = -1;
        m_running.store(false);
        return;
    }

    // 非阻塞 + select 等待
    timeval tv{};
    tv.tv_sec = 0;
    tv.tv_usec = 200000;  // 200ms
    ::setsockopt(m_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    m_thread = std::thread([this]() { receive_loop(); });

    log_info("listening on udp 0.0.0.0:" + std::to_string(m_port));
}

void UdpVideoReceiver::Stop() {
    bool expected = true;
    if (!m_running.compare_exchange_strong(expected, false)) {
        return;  // 没在跑
    }
    if (m_thread.joinable()) {
        m_thread.join();
    }
    if (m_sock >= 0) {
        ::close(m_sock);
        m_sock = -1;
    }
    log_info("stopped");
}

void UdpVideoReceiver::receive_loop() {
    std::vector<uint8_t> buf(64 * 1024);

    while (m_running.load()) {
        ssize_t n = ::recv(m_sock, buf.data(), buf.size(), 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                continue;  // 超时,继续检查 m_running
            }
            log_info(std::string("recv() failed: ") + std::strerror(errno));
            break;
        }
        if (n < static_cast<ssize_t>(sizeof(UdpVideoHeader))) {
            continue;  // 长度不够,丢弃
        }

        UdpVideoHeader hdr{};
        std::memcpy(&hdr, buf.data(), sizeof(hdr));
        const uint16_t frame_id       = ntohs(hdr.frame_id);
        const uint16_t frag_index     = ntohs(hdr.fragment_index);
        const uint32_t frame_total    = ntohl(hdr.frame_total_bytes);

        const uint8_t* payload     = buf.data() + sizeof(UdpVideoHeader);
        const size_t   payload_len = static_cast<size_t>(n) - sizeof(UdpVideoHeader);

        std::lock_guard<std::mutex> lk(m_bufMutex);

        // 新的一帧: 重置缓冲
        if (frame_id != m_curFrameId) {
            m_curFrameId   = frame_id;
            m_curFrameSize = frame_total;
            m_curFragIndex = 0;
            m_curFrameData.assign(frame_total, 0);
        }

        if (frame_total != m_curFrameSize) {
            // 中途帧大小变化,丢弃
            m_curFrameSize = frame_total;
            m_curFrameData.assign(frame_total, 0);
            m_curFragIndex = 0;
        }

        if (frag_index * 1024 + payload_len <= m_curFrameData.size()) {
            std::memcpy(m_curFrameData.data() + frag_index * 1024,
                        payload, payload_len);
        }

        m_curFragIndex++;

        // 简单判定: 累计接收字节数 >= frame_total 时认为帧完成
        if (m_curFragIndex * 1024 >= m_curFrameSize) {
            HevcFrame f;
            f.frame_id = m_curFrameId;
            f.data     = m_curFrameData;
            if (m_cb) {
                // 回调放到锁外避免死锁
                FrameCallback cb = m_cb;
                cb(f);
            }
            // 清空当前帧状态
            m_curFrameId   = 0;
            m_curFrameSize = 0;
            m_curFragIndex = 0;
            m_curFrameData.clear();
        }
    }
}
