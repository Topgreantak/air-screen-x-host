// Standalone self-test for the wire protocol. No gtest/vcpkg needed:
//   clang++ -std=c++20 test_protocol.cpp -o test_protocol && ./test_protocol
//   (or) cl /std:c++20 /EHsc test_protocol.cpp
// Exits non-zero on failure.
#include "../src/network/protocol.hpp"
#include <cassert>
#include <cstdio>

using namespace idsp;

static void test_video_header_roundtrip() {
    VideoHeader in{ .seq = 0xDEADBEEF, .timestampUs = 0x0123456789ABCDEFull,
                    .frameId = 42, .totalChunks = 7, .chunkIndex = 3,
                    .isKeyFrame = true, .isLastChunk = false, .payloadSize = 100 };
    uint8_t buf[VIDEO_HEADER + 100] = {};
    assert(writeVideoHeader(buf, in));
    VideoHeader out;
    assert(readVideoHeader(buf, out));
    assert(out.seq == in.seq);
    assert(out.timestampUs == in.timestampUs);
    assert(out.frameId == in.frameId);
    assert(out.totalChunks == in.totalChunks);
    assert(out.chunkIndex == in.chunkIndex);
    assert(out.isKeyFrame && !out.isLastChunk);
    assert(out.payloadSize == in.payloadSize);
}

static void test_bad_magic_rejected() {
    uint8_t buf[VIDEO_HEADER] = {};  // all-zero magic
    VideoHeader h;
    assert(!readVideoHeader(buf, h));
}

static void test_chunking_reassembles() {
    // frame slightly over 2 chunks → expect 3 chunks
    std::vector<uint8_t> frame(MAX_CHUNK * 2 + 5);
    for (size_t i = 0; i < frame.size(); ++i) frame[i] = static_cast<uint8_t>(i);  // 0,1,2,... wraps

    auto packets = chunkFrame(frame, 1000, 77, 999, /*key=*/true);
    assert(packets.size() == 3);

    std::vector<uint8_t> reassembled;
    for (size_t i = 0; i < packets.size(); ++i) {
        VideoHeader h;
        assert(readVideoHeader(packets[i], h));
        assert(h.frameId == 77);
        assert(h.totalChunks == 3);
        assert(h.chunkIndex == i);
        assert(h.seq == 1000 + i);
        assert(h.isKeyFrame);
        assert(h.isLastChunk == (i == packets.size() - 1));
        assert(h.payloadSize == packets[i].size() - VIDEO_HEADER);
        reassembled.insert(reassembled.end(),
                           packets[i].begin() + VIDEO_HEADER, packets[i].end());
    }
    assert(reassembled == frame);
}

static void test_input_roundtrip() {
    InputPacket in;
    in.timestampUs = 1720000000000000ull;
    in.type = TouchType::Move;
    in.touches = { {1, 0.25f, 0.75f}, {2, 0.5f, 0.5f} };

    uint8_t buf[64];
    size_t n = writeInputPacket(buf, in);
    assert(n == inputPacketSize(2));

    InputPacket out;
    assert(readInputPacket(std::span<const uint8_t>(buf, n), out));
    assert(out.timestampUs == in.timestampUs);
    assert(out.type == TouchType::Move);
    assert(out.touches.size() == 2);
    assert(out.touches[0].id == 1 && out.touches[0].x == 0.25f && out.touches[0].y == 0.75f);
    assert(out.touches[1].id == 2 && out.touches[1].x == 0.5f  && out.touches[1].y == 0.5f);
}

static void test_input_truncation_rejected() {
    // claims 3 touches but buffer only holds header → must reject, not overread
    uint8_t buf[14] = {};
    detail::put_u32(buf, INPUT_MAGIC);
    buf[13] = 3;
    InputPacket out;
    assert(!readInputPacket(buf, out));
}

int main() {
    test_video_header_roundtrip();
    test_bad_magic_rejected();
    test_chunking_reassembles();
    test_input_roundtrip();
    test_input_truncation_rejected();
    std::puts("protocol self-test: ALL PASS");
    return 0;
}
