#pragma once
// Control server (TCP, port 7656). README section 4.3 / section 5.
// iOS-initiated, LAN-only, deny-by-default. Drives the PairingManager (R6): on PAIR_REQUEST
// it asks the host (tray) to Accept/Deny via a blocking prompt callback, then issues a
// CSPRNG session token and replies CONFIG_ACK. One client at a time.

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include "net_common.hpp"
#include "pairing.hpp"
#include "ctrl_protocol.hpp"

namespace idsp {

// Authoritative stream params the host supplies for CONFIG_ACK (R1 — Windows owns config).
struct AckParams {
    std::string mode = "extend", aspect = "16:9";
    uint32_t    width = 1920, height = 1080, fps = 60;
};

class CtrlServer {
public:
    static constexpr uint16_t CTRL_PORT = 7656;

    // prompt: blocks, returns true = Accept. prefs: iOS FPS/display prefs. ackInfo: current config.
    using PromptFn   = std::function<bool(const std::string& deviceName, const std::string& deviceId)>;
    using PrefsFn    = std::function<void(const ctrl::SetPrefs&)>;
    using AckInfoFn  = std::function<AckParams()>;
    using PairedFn   = std::function<void(const std::string& clientIp)>;  // start streaming
    using UnpairedFn = std::function<void()>;                             // stop streaming

    CtrlServer(uint16_t port, PromptFn prompt, PrefsFn onPrefs, AckInfoFn ackInfo);

    void setPairedHandlers(PairedFn onPaired, UnpairedFn onUnpaired) {
        m_onPaired = std::move(onPaired); m_onUnpaired = std::move(onUnpaired);
    }
    ~CtrlServer();
    CtrlServer(const CtrlServer&) = delete;

    bool start();
    void stop();

    bool isPaired() { return m_pairing.state() == PairState::Paired; }

private:
    void loop();
    void serveClient(SOCKET client, const std::string& clientIp);

    WsaGuard          m_wsa;
    SOCKET            m_listen{INVALID_SOCKET};
    uint16_t          m_port;
    PromptFn          m_prompt;
    PrefsFn           m_onPrefs;
    AckInfoFn         m_ackInfo;
    PairedFn          m_onPaired;
    UnpairedFn        m_onUnpaired;
    PairingManager    m_pairing;
    std::atomic<bool> m_running{false};
    std::thread       m_thread;
};

}  // namespace idsp
