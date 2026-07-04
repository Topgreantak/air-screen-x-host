#include "encoder.hpp"
#include "mf_encoder.hpp"

namespace idsp {

std::unique_ptr<IEncoder> EncoderFactory::createBest(const EncoderConfig& cfg) {
    // ponytail: NVENC/QSV/AMF backends need their vendor SDKs (not vendored here). Media Foundation
    // already exposes hardware H.264 via MFT_ENUM_FLAG_HARDWARE, so MF is the single fallback that
    // also uses the GPU when present. Add a dedicated NVENC path only if MF's latency is measurably
    // worse on NVIDIA — see wiki [[capture-encode]].
    if (auto mf = MFEncoder::create(cfg)) return mf;
    return nullptr;  // README Notes 6: never exit — caller logs + reports EXIT_NO_ENCODER
}

}  // namespace idsp
