// Self-test: config JSON serde + input coordinate math. Asserts only.
#include "../src/config/config.hpp"
#include "../src/input/input_injector.hpp"
#include "../src/encoder/color_convert.hpp"
#include <cassert>
#include <cstdio>
#include <vector>

using namespace idsp;

static void test_config_parse() {
    const char* txt = R"({
      "host": {"videoPort": 8000, "bindAddress": "127.0.0.1"},
      "stream": {"defaultMode": "mirror", "defaultAspectRatio": "4:3",
                 "defaultResolution": {"width": 1600, "height": 1200},
                 "fps": 30, "bitrateKbps": 8000, "encoderPreference": "nvenc"},
      "input": {"enableScroll": false, "scrollSensitivity": 2.0}
    })";
    bool ok = false;
    Config c = configFromJson(txt, ok);
    assert(ok);
    assert(c.host.videoPort == 8000);
    assert(c.host.inputPort == 7655);          // untouched default
    assert(c.host.bindAddress == "127.0.0.1");
    assert(c.stream.mode == Mode::Mirror);
    assert(c.stream.aspectRatio == "4:3");
    assert(c.stream.resolution.width == 1600 && c.stream.resolution.height == 1200);
    assert(c.stream.fps == 30 && c.stream.bitrateKbps == 8000);
    assert(c.stream.encoderPreference == "nvenc");
    assert(c.input.enableTouch == true);       // default
    assert(c.input.enableScroll == false);
    assert(c.input.scrollSensitivity == 2.0f);
}

static void test_config_bad_json() {
    bool ok = true;
    Config c = configFromJson("{ not json", ok);
    assert(!ok);
    assert(c.stream.fps == 60);                // defaults returned
    assert(c.host.videoPort == 7654);
}

static void test_config_roundtrip() {
    Config a;
    a.stream.mode = Mode::Mirror;
    a.stream.resolution = {2560, 1440};
    a.host.ctrlPort = 9000;
    bool ok = false;
    Config b = configFromJson(configToJson(a), ok);
    assert(ok);
    assert(b.stream.mode == Mode::Mirror);
    assert(b.stream.resolution.width == 2560 && b.stream.resolution.height == 1440);
    assert(b.host.ctrlPort == 9000);
}

static void test_input_coords() {
    auto a = normalizedToAbsolute(0.f, 0.f);
    assert(a.x == 0 && a.y == 0);
    auto b = normalizedToAbsolute(1.f, 1.f);
    assert(b.x == 65535 && b.y == 65535);
    auto c = normalizedToAbsolute(0.5f, 0.5f);
    assert(c.x == 32768 && c.y == 32768);
    auto d = normalizedToAbsolute(-1.f, 2.f);   // clamped
    assert(d.x == 0 && d.y == 65535);
}

static void fillBGRA(std::vector<uint8_t>& buf, uint8_t b, uint8_t g, uint8_t r) {
    for (size_t i = 0; i < buf.size(); i += 4) { buf[i]=b; buf[i+1]=g; buf[i+2]=r; buf[i+3]=255; }
}

static void test_color_convert() {
    const uint32_t w = 2, h = 2;
    std::vector<uint8_t> bgra(w * h * 4);
    uint8_t Y[w*h], UV[w*(h/2)];

    fillBGRA(bgra, 255, 255, 255);                 // white
    bgraToNV12(bgra.data(), w, h, w*4, Y, UV);
    assert(Y[0] == 235 && Y[3] == 235);            // limited-range white luma
    assert(UV[0] == 128 && UV[1] == 128);          // neutral chroma

    fillBGRA(bgra, 0, 0, 255);                      // red
    bgraToNV12(bgra.data(), w, h, w*4, Y, UV);
    assert(Y[0] == 82);                             // red luma
    assert(UV[1] == 240);                           // red → high V
}

int main() {
    test_config_parse();
    test_config_bad_json();
    test_config_roundtrip();
    test_input_coords();
    test_color_convert();
    std::puts("config/input/color self-test: ALL PASS");
    return 0;
}
