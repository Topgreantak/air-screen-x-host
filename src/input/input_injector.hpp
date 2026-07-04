#pragma once
// Touch → Windows mouse injection (SendInput). README section 5.7.
// Normalized 0..1 coords (per virtual resolution) → absolute mouse. The coordinate
// math is a pure free function (`normalizedToAbsolute`) so it is unit-testable without Win32.

#include <cstdint>

namespace idsp {

struct TouchEvent {
    enum class Type : uint8_t { Down = 0x01, Move = 0x02, Up = 0x03, Scroll = 0x04 };
    Type     type;
    uint8_t  touchId;
    float    normX;   // 0.0-1.0 over virtual resolution
    float    normY;
    uint64_t timestampUs;
};

// SendInput MOUSEEVENTF_ABSOLUTE space is 0..65535 across the target surface.
// Pure, clamped. Returns {absX, absY}.
struct AbsPoint { int32_t x, y; };
inline AbsPoint normalizedToAbsolute(float nx, float ny) {
    auto clamp01 = [](float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); };
    return { static_cast<int32_t>(clamp01(nx) * 65535.0f + 0.5f),
             static_cast<int32_t>(clamp01(ny) * 65535.0f + 0.5f) };
}

class InputInjector {
public:
    explicit InputInjector(bool wholeVirtualDesktop = false)
        : m_virtualDesktop(wholeVirtualDesktop) {}

    // Inject one event. Single-pointer → mouse. ponytail: multi-touch gestures map to the
    // first touch only for now; per-touch injection needs the Win32 Touch Injection API
    // (InitializeTouchInjection) — add if multi-finger gestures are required.
    void injectTouch(const TouchEvent& e) const;

private:
    bool m_virtualDesktop;  // map across all monitors vs primary only
};

}  // namespace idsp
