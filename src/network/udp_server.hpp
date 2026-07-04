#pragma once
// Video stream sender (UDP, port 7654). README section 5.6.
// SendFrame() chunks an H.264 NAL in place (reuses protocol.hpp headers) and sends MTU-safe
// packets. Thread-safe: SendFrame is called from the encoder thread. Socket via RAII.

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include "net_common.hpp"

namespace idsp {

class UdpServer {
public:
    static constexpr uint16_t VIDEO_PORT = 7654;

    explicit UdpServer(uint16_t port = VIDEO_PORT);
    ~UdpServer();
    UdpServer(const UdpServer&) = delete;
    UdpServer& operator=(const UdpServer&) = delete;

    bool bind();
    void setClientEndpoint(const std::string& ip, uint16_t port);
    bool hasClient() const { return m_hasClient.load(std::memory_order_acquire); }

    // Chunk + send. Thread-safe. Returns false if no client or a send fails.
    bool sendFrame(const uint8_t* data, size_t size, bool isKeyFrame, uint64_t timestampUs);

private:
    WsaGuard              m_wsa;
    SOCKET                m_socket{INVALID_SOCKET};
    uint16_t              m_port;
    sockaddr_in           m_clientAddr{};
    std::atomic<bool>     m_hasClient{false};
    std::mutex            m_sendMutex;
    std::atomic<uint32_t> m_seqNum{0};
    std::atomic<uint32_t> m_frameId{0};
};

}  // namespace idsp
