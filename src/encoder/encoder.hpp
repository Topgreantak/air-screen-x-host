#pragma once
// H.264 encoder interface. README section 5.5. Factory picks the best available backend.
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace idsp {

using EncodedCallback = std::function<void(
    const uint8_t* data, size_t size, bool isKeyFrame, uint64_t timestampUs)>;

struct EncoderConfig {
    uint32_t width = 1920, height = 1080, fps = 60;
    uint32_t bitrateKbps = 12000;
    uint32_t keyframeIntervalSec = 2;
    bool     enableLowLatency = true;
};

class IEncoder {
public:
    virtual ~IEncoder() = default;
    // Encode one BGRA frame (stride = width*4). Emits NAL(s) via cb (may be called 0+ times).
    virtual bool encodeFrame(const uint8_t* bgra, size_t size,
                             uint64_t timestampUs, const EncodedCallback& cb) = 0;
    virtual std::string name() const = 0;
};

class EncoderFactory {
public:
    // Try NVENC → QSV/AMF → Media Foundation. Never returns null on a sane system:
    // Media Foundation H.264 is the guaranteed fallback (README Notes 6).
    static std::unique_ptr<IEncoder> createBest(const EncoderConfig& cfg);
};

}  // namespace idsp
