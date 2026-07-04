#pragma once
// Media Foundation H.264 encoder — the guaranteed software/hardware fallback.
// Wraps the H.264 MFT. Compile-verified; runtime needs a working MF stack.
#include "encoder.hpp"
#include <wrl/client.h>
#include <mfidl.h>

namespace idsp {

class MFEncoder : public IEncoder {
public:
    static std::unique_ptr<MFEncoder> create(const EncoderConfig& cfg);
    ~MFEncoder() override;

    bool encodeFrame(const uint8_t* bgra, size_t size,
                     uint64_t timestampUs, const EncodedCallback& cb) override;
    std::string name() const override { return "MediaFoundation-H264"; }

private:
    explicit MFEncoder(const EncoderConfig& cfg) : m_cfg(cfg) {}
    bool init();
    void drainOutput(const EncodedCallback& cb);

    EncoderConfig m_cfg;
    Microsoft::WRL::ComPtr<IMFTransform> m_mft;
    bool     m_streaming = false;
    uint64_t m_frameIndex = 0;
};

}  // namespace idsp
