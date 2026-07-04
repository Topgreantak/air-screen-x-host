#pragma once
// Touch/input receiver (UDP, port 7655). README section 5.6.
// Binds, runs a recv loop on its own thread, parses IDIP packets (protocol.hpp),
// and hands each touch to a callback. RAII socket + joined thread.

#include <atomic>
#include <cstdint>
#include <functional>
#include <thread>
#include "net_common.hpp"
#include "protocol.hpp"

namespace idsp {

class InputReceiver {
public:
    static constexpr uint16_t INPUT_PORT = 7655;

    // Callback gets a parsed input packet (event type + touches). Called from the recv thread.
    using Callback = std::function<void(const InputPacket&)>;

    explicit InputReceiver(uint16_t port = INPUT_PORT);
    ~InputReceiver();
    InputReceiver(const InputReceiver&) = delete;

    bool bind();
    bool start(Callback cb);
    void stop();  // blocks until the recv thread joins

private:
    void loop();

    WsaGuard          m_wsa;
    SOCKET            m_socket{INVALID_SOCKET};
    uint16_t          m_port;
    Callback          m_cb;
    std::atomic<bool> m_running{false};
    std::thread       m_thread;
};

}  // namespace idsp
