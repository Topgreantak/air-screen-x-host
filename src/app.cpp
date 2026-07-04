#include "app.hpp"
#include "encoder/color_convert.hpp"
#include "network/protocol.hpp"

namespace idsp {

App::App() {
    m_config   = loadConfig();
    m_injector = std::make_unique<InputInjector>();

    // Video sender.
    m_video = std::make_unique<UdpServer>(m_config.host.videoPort);
    m_video->bind();

    // Input receiver → mouse injection.
    m_input = std::make_unique<InputReceiver>(m_config.host.inputPort);
    if (m_input->bind()) {
        m_input->start([this](const InputPacket& pkt) {
            if (!m_config.input.enableTouch) return;
            for (const auto& t : pkt.touches) {
                TouchEvent e{ static_cast<TouchEvent::Type>(pkt.type), t.id, t.x, t.y, pkt.timestampUs };
                m_injector->injectTouch(e);
            }
        });
    }

    // Control server: pairing prompt via tray (R6), prefs, ack info, paired/unpaired hooks.
    m_ctrl = std::make_unique<CtrlServer>(
        m_config.host.ctrlPort,
        [this](const std::string& name, const std::string& id) {
            return m_tray.promptAcceptDeny(name, id);
        },
        [this](const ctrl::SetPrefs& p) { if (p.fps) m_config.stream.fps = p.fps; },
        [this]() -> AckParams {
            return { modeName(m_config.stream.mode), m_config.stream.aspectRatio,
                     m_config.stream.resolution.width, m_config.stream.resolution.height,
                     m_config.stream.fps };
        });
    m_ctrl->setPairedHandlers([this](const std::string& ip) { onPaired(ip); },
                              [this] { onUnpaired(); });
    m_ctrl->start();
}

App::~App() { onUnpaired(); }

void App::onPaired(const std::string& clientIp) {
    std::lock_guard<std::mutex> lk(m_pipeMutex);
    if (m_streaming.load()) return;

    m_video->setClientEndpoint(clientIp, m_config.host.videoPort);

    // Mirror mode: capture the primary output. Extend (VDD) is a TODO (stub returns unavailable).
    auto cap = DXGICapture::create(/*outputIndex=*/0);
    if (!cap) return;  // no display / access lost — nothing to stream
    m_capture = std::make_unique<DXGICapture>(std::move(*cap));

    m_idle.store(false);
    m_streaming.store(true);
    m_capture->start(
        [this](const uint8_t* bgra, uint32_t w, uint32_t h, uint32_t stride, uint64_t ts) {
            onCaptureFrame(bgra, w, h, stride, ts);
        },
        [this] { m_idle.store(true); },     // R4: static desktop → stop sending
        [this] { m_idle.store(false); });   // motion → resume
}

void App::onUnpaired() {
    std::lock_guard<std::mutex> lk(m_pipeMutex);
    m_streaming.store(false);
    if (m_capture) { m_capture->stop(); m_capture.reset(); }
    m_encoder.reset();
}

void App::onCaptureFrame(const uint8_t* bgra, uint32_t w, uint32_t h, uint32_t stride, uint64_t ts) {
    if (m_idle.load(std::memory_order_acquire)) return;   // idle-stop
    if ((w & 1) || (h & 1)) return;                       // NV12 needs even dims

    // Lazily create the encoder sized to the actual capture resolution.
    if (!m_encoder) {
        EncoderConfig ec{ w, h, m_config.stream.fps, m_config.stream.bitrateKbps,
                          m_config.stream.keyframeIntervalSec, true };
        m_encoder = EncoderFactory::createBest(ec);
        if (!m_encoder) return;  // README Notes 6: never crash; just skip until an encoder exists
    }

    const size_t ySize = size_t(w) * h;
    const size_t total = ySize + ySize / 2;
    if (m_nv12.size() != total) m_nv12.resize(total);
    bgraToNV12(bgra, w, h, stride, m_nv12.data(), m_nv12.data() + ySize);

    m_encoder->encodeFrame(m_nv12.data(), total, ts,
        [this](const uint8_t* nal, size_t size, bool key, uint64_t t) {
            m_video->sendFrame(nal, size, key, t);
        });
}

int App::run() {
    if (!m_tray.install(L"iDisplay — waiting for connection")) return EXIT_BIND_FAIL;
    m_tray.onQuit = [this] { onUnpaired(); };
    m_tray.runMessageLoop();
    return EXIT_OK;
}

}  // namespace idsp
