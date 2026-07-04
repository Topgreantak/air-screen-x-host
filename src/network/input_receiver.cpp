#include "input_receiver.hpp"
#include "lan.hpp"
#include <array>

namespace idsp {

InputReceiver::InputReceiver(uint16_t port) : m_port(port) {}

InputReceiver::~InputReceiver() { stop(); }

bool InputReceiver::bind() {
    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket == INVALID_SOCKET) return false;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(m_port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    return ::bind(m_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
}

bool InputReceiver::start(Callback cb) {
    if (m_socket == INVALID_SOCKET) return false;
    m_cb = std::move(cb);
    m_running.store(true, std::memory_order_release);
    m_thread = std::thread(&InputReceiver::loop, this);
    return true;
}

void InputReceiver::stop() {
    m_running.store(false, std::memory_order_release);
    if (m_socket != INVALID_SOCKET) { closesocket(m_socket); m_socket = INVALID_SOCKET; }
    if (m_thread.joinable()) m_thread.join();
}

void InputReceiver::loop() {
    std::array<uint8_t, 2048> buf;
    while (m_running.load(std::memory_order_acquire)) {
        sockaddr_in from{}; int fromLen = sizeof(from);
        int n = recvfrom(m_socket, reinterpret_cast<char*>(buf.data()),
                         static_cast<int>(buf.size()), 0,
                         reinterpret_cast<sockaddr*>(&from), &fromLen);
        if (n <= 0) continue;

        // R5: only accept input from a LAN peer.
        if (!lan::isPrivateIPv4(ntohl(from.sin_addr.s_addr))) continue;

        InputPacket pkt;
        if (readInputPacket(std::span<const uint8_t>(buf.data(), static_cast<size_t>(n)), pkt)) {
            if (m_cb) m_cb(pkt);
        }  // malformed → dropped (trust boundary, R2)
    }
}

}  // namespace idsp
