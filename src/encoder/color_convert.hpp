#pragma once
// BGRA → NV12 (BT.601 limited range). The MF H.264 encoder takes NV12; DXGI capture gives BGRA.
// Pure + unit-testable. ponytail: CPU convert is the simple correct path but costs cycles —
// for the perf/no-lag pillars, move this to a GPU shader or a DXGI video-processor MFT once the
// pipeline runs. This exists so the pipeline is correct end-to-end today.

#include <cstdint>

namespace idsp {

// dstY: width*height bytes. dstUV: width*(height/2) bytes (interleaved U,V), 2x2 subsampled.
// srcStride = bytes per BGRA row. width/height must be even for NV12.
inline void bgraToNV12(const uint8_t* bgra, uint32_t width, uint32_t height, uint32_t srcStride,
                       uint8_t* dstY, uint8_t* dstUV) {
    auto clamp8 = [](int v) -> uint8_t { return v < 0 ? 0 : (v > 255 ? 255 : uint8_t(v)); };

    for (uint32_t y = 0; y < height; ++y) {
        const uint8_t* row = bgra + y * srcStride;
        uint8_t* yrow = dstY + y * width;
        for (uint32_t x = 0; x < width; ++x) {
            const uint8_t b = row[x * 4 + 0], g = row[x * 4 + 1], r = row[x * 4 + 2];
            yrow[x] = clamp8((66 * r + 129 * g + 25 * b + 128 >> 8) + 16);
        }
    }
    // 2x2 chroma: sample the top-left pixel of each block (nearest — cheap).
    for (uint32_t y = 0; y < height; y += 2) {
        uint8_t* uv = dstUV + (y / 2) * width;
        const uint8_t* row = bgra + y * srcStride;
        for (uint32_t x = 0; x < width; x += 2) {
            const uint8_t b = row[x * 4 + 0], g = row[x * 4 + 1], r = row[x * 4 + 2];
            uv[x + 0] = clamp8((-38 * r - 74 * g + 112 * b + 128 >> 8) + 128);  // U
            uv[x + 1] = clamp8((112 * r - 94 * g - 18 * b + 128 >> 8) + 128);   // V
        }
    }
}

}  // namespace idsp
