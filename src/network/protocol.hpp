#pragma once
// iDisplay wire protocol — video (IDSP) + input (IDIP) packet (de)serialization.
// Pure logic, no Win32/socket deps → unit-testable standalone. See README §4.1, §4.2.
// All multi-byte fields are big-endian (network byte order).

#include <cstdint>
#include <cstring>
#include <bit>
#include <span>
#include <vector>

namespace idsp {

// ---- video (port 7654) ----
inline constexpr uint32_t VIDEO_MAGIC   = 0x49445350;  // "IDSP"
inline constexpr size_t   VIDEO_HEADER  = 29;
inline constexpr size_t   MAX_PACKET    = 1400;               // MTU-safe (1500 - IP/UDP)
inline constexpr size_t   MAX_CHUNK     = MAX_PACKET - VIDEO_HEADER;  // 1371

enum VideoFlag : uint8_t { FLAG_KEYFRAME = 0x01, FLAG_LAST_CHUNK = 0x02 };

struct VideoHeader {
    uint32_t seq         = 0;
    uint64_t timestampUs = 0;
    uint32_t frameId     = 0;
    uint16_t totalChunks = 0;
    uint16_t chunkIndex  = 0;
    bool     isKeyFrame  = false;
    bool     isLastChunk = false;
    uint32_t payloadSize = 0;  // bytes of NAL payload in THIS chunk
};

// ---- input (port 7655) ----
inline constexpr uint32_t INPUT_MAGIC = 0x49444950;  // "IDIP"

enum class TouchType : uint8_t { Down = 0x01, Move = 0x02, Up = 0x03, Scroll = 0x04 };

struct TouchPoint {
    uint8_t id = 0;
    float   x  = 0.0f;  // normalized 0.0-1.0
    float   y  = 0.0f;
};

struct InputPacket {
    uint64_t                timestampUs = 0;
    TouchType               type        = TouchType::Down;
    std::vector<TouchPoint> touches;
};

// ---------- big-endian put/get helpers ----------
namespace detail {

inline void put_u16(uint8_t* p, uint16_t v) { p[0] = v >> 8; p[1] = v & 0xFF; }
inline void put_u32(uint8_t* p, uint32_t v) {
    p[0] = v >> 24; p[1] = (v >> 16) & 0xFF; p[2] = (v >> 8) & 0xFF; p[3] = v & 0xFF;
}
inline void put_u64(uint8_t* p, uint64_t v) {
    for (int i = 0; i < 8; ++i) p[i] = static_cast<uint8_t>(v >> (56 - 8 * i));
}
inline uint16_t get_u16(const uint8_t* p) { return (uint16_t(p[0]) << 8) | p[1]; }
inline uint32_t get_u32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}
inline uint64_t get_u64(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v = (v << 8) | p[i];
    return v;
}
inline void put_f32(uint8_t* p, float f) { put_u32(p, std::bit_cast<uint32_t>(f)); }
inline float get_f32(const uint8_t* p)   { return std::bit_cast<float>(get_u32(p)); }

}  // namespace detail

// ---------- video header ----------
// Writes 29 bytes into out. Returns false if out is too small.
inline bool writeVideoHeader(std::span<uint8_t> out, const VideoHeader& h) {
    if (out.size() < VIDEO_HEADER) return false;
    uint8_t* p = out.data();
    detail::put_u32(p + 0,  VIDEO_MAGIC);
    detail::put_u32(p + 4,  h.seq);
    detail::put_u64(p + 8,  h.timestampUs);
    detail::put_u32(p + 16, h.frameId);
    detail::put_u16(p + 20, h.totalChunks);
    detail::put_u16(p + 22, h.chunkIndex);
    uint8_t flags = 0;
    if (h.isKeyFrame)  flags |= FLAG_KEYFRAME;
    if (h.isLastChunk) flags |= FLAG_LAST_CHUNK;
    p[24] = flags;
    detail::put_u32(p + 25, h.payloadSize);
    return true;
}

// Parses header from in. Returns false if too small or magic mismatch.
inline bool readVideoHeader(std::span<const uint8_t> in, VideoHeader& h) {
    if (in.size() < VIDEO_HEADER) return false;
    const uint8_t* p = in.data();
    if (detail::get_u32(p + 0) != VIDEO_MAGIC) return false;
    h.seq         = detail::get_u32(p + 4);
    h.timestampUs = detail::get_u64(p + 8);
    h.frameId     = detail::get_u32(p + 16);
    h.totalChunks = detail::get_u16(p + 20);
    h.chunkIndex  = detail::get_u16(p + 22);
    uint8_t flags = p[24];
    h.isKeyFrame  = flags & FLAG_KEYFRAME;
    h.isLastChunk = flags & FLAG_LAST_CHUNK;
    h.payloadSize = detail::get_u32(p + 25);
    // guard against malformed payloadSize (trust boundary — R2)
    if (h.payloadSize > MAX_CHUNK || in.size() < VIDEO_HEADER + h.payloadSize) return false;
    return true;
}

// Splits one H.264 NAL frame into MTU-safe packets (header + payload each).
// ponytail: allocates a vector-of-vectors for testability; UdpServer will instead
// chunk-and-send in place to avoid the copy. Upgrade path: swap to a callback sink.
inline std::vector<std::vector<uint8_t>>
chunkFrame(std::span<const uint8_t> nal, uint32_t seqStart, uint32_t frameId,
           uint64_t timestampUs, bool isKeyFrame) {
    const size_t total = (nal.size() + MAX_CHUNK - 1) / MAX_CHUNK;
    const size_t chunks = total == 0 ? 1 : total;  // always at least one (may be empty payload)

    std::vector<std::vector<uint8_t>> out;
    out.reserve(chunks);
    for (size_t i = 0; i < chunks; ++i) {
        const size_t off = i * MAX_CHUNK;
        const size_t len = std::min(MAX_CHUNK, nal.size() - std::min(off, nal.size()));
        VideoHeader h;
        h.seq         = seqStart + static_cast<uint32_t>(i);
        h.timestampUs = timestampUs;
        h.frameId     = frameId;
        h.totalChunks = static_cast<uint16_t>(chunks);
        h.chunkIndex  = static_cast<uint16_t>(i);
        h.isKeyFrame  = isKeyFrame;
        h.isLastChunk = (i + 1 == chunks);
        h.payloadSize = static_cast<uint32_t>(len);

        std::vector<uint8_t> pkt(VIDEO_HEADER + len);
        writeVideoHeader(pkt, h);
        if (len) std::memcpy(pkt.data() + VIDEO_HEADER, nal.data() + off, len);
        out.push_back(std::move(pkt));
    }
    return out;
}

// ---------- input packet ----------
inline size_t inputPacketSize(size_t touchCount) { return 14 + 9 * touchCount; }

// Serializes into out. Returns bytes written, or 0 if out too small / too many touches.
inline size_t writeInputPacket(std::span<uint8_t> out, const InputPacket& ev) {
    if (ev.touches.size() > 255) return 0;
    const size_t need = inputPacketSize(ev.touches.size());
    if (out.size() < need) return 0;
    uint8_t* p = out.data();
    detail::put_u32(p + 0, INPUT_MAGIC);
    detail::put_u64(p + 4, ev.timestampUs);
    p[12] = static_cast<uint8_t>(ev.type);
    p[13] = static_cast<uint8_t>(ev.touches.size());
    size_t o = 14;
    for (const auto& t : ev.touches) {
        p[o] = t.id;
        detail::put_f32(p + o + 1, t.x);
        detail::put_f32(p + o + 5, t.y);
        o += 9;
    }
    return need;
}

// Parses input packet. Returns false on bad magic / truncation.
inline bool readInputPacket(std::span<const uint8_t> in, InputPacket& ev) {
    if (in.size() < 14) return false;
    const uint8_t* p = in.data();
    if (detail::get_u32(p + 0) != INPUT_MAGIC) return false;
    ev.timestampUs = detail::get_u64(p + 4);
    ev.type = static_cast<TouchType>(p[12]);
    const uint8_t count = p[13];
    if (in.size() < inputPacketSize(count)) return false;  // trust boundary — R2
    ev.touches.clear();
    ev.touches.reserve(count);
    size_t o = 14;
    for (uint8_t i = 0; i < count; ++i) {
        ev.touches.push_back({ p[o], detail::get_f32(p + o + 1), detail::get_f32(p + o + 5) });
        o += 9;
    }
    return true;
}

}  // namespace idsp
