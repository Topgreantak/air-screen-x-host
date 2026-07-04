#include "udp_server.hpp"
#include "protocol.hpp"
#include <vector>

namespace idsp {

UdpServer::UdpServer(uint16_t port) : m_port(port) {}

UdpServer::~UdpServer() {
    if (m_socket != INVALID_SOCKET) closesocket(m_socket);
}

bool UdpServer::bind() {
    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket == INVALID_SOCKET) return false;
    // sender socket: no explicit bind needed; larger send buffer for bursty keyframes
    int buf = 1 << 20;
    setsockopt(m_socket, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char*>(&buf), sizeof(buf));
    return true;
}

void UdpServer::setClientEndpoint(const std::string& ip, uint16_t port) {
    std::lock_guard<std::mutex> lk(m_sendMutex);
    m_clientAddr = {};
    m_clientAddr.sin_family = AF_INET;
    m_clientAddr.sin_port   = htons(port);
    inet_pton(AF_INET, ip.c_str(), &m_clientAddr.sin_addr);
    m_hasClient.store(true, std::memory_order_release);
}

bool UdpServer::sendFrame(const uint8_t* data, size_t size, bool isKeyFrame, uint64_t timestampUs) {
    if (!m_hasClient.load(std::memory_order_acquire) || m_socket == INVALID_SOCKET) return false;

    const size_t chunks = size == 0 ? 1 : (size + idsp::MAX_CHUNK - 1) / idsp::MAX_CHUNK;
    const uint32_t frameId = m_frameId.fetch_add(1, std::memory_order_relaxed);

    // ponytail: one reusable stack buffer per packet; chunk in place (no vector-of-vectors copy).
    uint8_t pkt[idsp::MAX_PACKET];
    std::lock_guard<std::mutex> lk(m_sendMutex);
    bool okAll = true;
    for (size_t i = 0; i < chunks; ++i) {
        const size_t off = i * idsp::MAX_CHUNK;
        const size_t len = std::min(idsp::MAX_CHUNK, size - std::min(off, size));
        VideoHeader h;
        h.seq         = m_seqNum.fetch_add(1, std::memory_order_relaxed);
        h.timestampUs = timestampUs;
        h.frameId     = frameId;
        h.totalChunks = static_cast<uint16_t>(chunks);
        h.chunkIndex  = static_cast<uint16_t>(i);
        h.isKeyFrame  = isKeyFrame;
        h.isLastChunk = (i + 1 == chunks);
        h.payloadSize = static_cast<uint32_t>(len);
        writeVideoHeader(std::span<uint8_t>(pkt, idsp::VIDEO_HEADER), h);
        if (len) std::memcpy(pkt + idsp::VIDEO_HEADER, data + off, len);

        int sent = sendto(m_socket, reinterpret_cast<char*>(pkt),
                          static_cast<int>(idsp::VIDEO_HEADER + len), 0,
                          reinterpret_cast<sockaddr*>(&m_clientAddr), sizeof(m_clientAddr));
        if (sent == SOCKET_ERROR) okAll = false;
    }
    return okAll;
}

}  // namespace idsp
