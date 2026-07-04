#pragma once
// Application root — owns every component, wires the pipeline. README section 5.8.
// Destruction = reverse construction (RAII). Mirror mode works today; Extend needs the
// Parsec VDD SDK (see [[capture-encode]]).

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>
#include "config/config.hpp"
#include "display/dxgi_capture.hpp"
#include "encoder/encoder.hpp"
#include "network/udp_server.hpp"
#include "network/input_receiver.hpp"
#include "network/ctrl_server.hpp"
#include "input/input_injector.hpp"
#include "ui/tray.hpp"

namespace idsp {

class App {
public:
    static constexpr int EXIT_OK          = 0;
    static constexpr int EXIT_NO_ENCODER  = 2;
    static constexpr int EXIT_BIND_FAIL   = 3;

    App();
    ~App();

    int run();  // blocks on the tray message loop until Quit

private:
    void onPaired(const std::string& clientIp);
    void onUnpaired();
    void onCaptureFrame(const uint8_t* bgra, uint32_t w, uint32_t h, uint32_t stride, uint64_t ts);

    Config m_config;
    Tray   m_tray;

    std::unique_ptr<UdpServer>     m_video;
    std::unique_ptr<InputReceiver> m_input;
    std::unique_ptr<CtrlServer>    m_ctrl;
    std::unique_ptr<InputInjector> m_injector;

    // stream pipeline (created on pair, torn down on unpair)
    std::unique_ptr<DXGICapture> m_capture;
    std::unique_ptr<IEncoder>    m_encoder;
    std::vector<uint8_t>         m_nv12;      // reused NV12 scratch
    std::mutex                   m_pipeMutex;
    std::atomic<bool>            m_idle{false};    // R4 idle-stop: skip sending while static
    std::atomic<bool>            m_streaming{false};
};

}  // namespace idsp
