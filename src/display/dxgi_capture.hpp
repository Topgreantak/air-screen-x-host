#pragma once
// DXGI Desktop Duplication capture (mirror mode = primary output). README section 5.4.
// Delivers BGRA frames on a capture thread. Idle-stop (R4): when the desktop is static,
// fires onIdle once and stops delivering frames; the next real frame fires onResume.
// Compile-verified; runtime needs a display.

#include <atomic>
#include <cstdint>
#include <functional>
#include <optional>
#include <thread>
#include <d3d11.h>
#include <dxgi1_5.h>
#include <wrl/client.h>

namespace idsp {

using FrameCallback = std::function<void(
    const uint8_t* bgra, uint32_t width, uint32_t height, uint32_t stride, uint64_t timestampUs)>;

class DXGICapture {
public:
    // outputIndex: which monitor (0 = primary, for mirror). Returns nullopt on failure.
    static std::optional<DXGICapture> create(int outputIndex = 0);

    ~DXGICapture();
    DXGICapture(const DXGICapture&) = delete;
    DXGICapture& operator=(const DXGICapture&) = delete;
    DXGICapture(DXGICapture&&) noexcept;

    // idleAfterMs: consecutive static time before declaring idle (R4). 0 disables idle-stop.
    bool start(FrameCallback onFrame,
               std::function<void()> onIdle = {},
               std::function<void()> onResume = {},
               uint32_t idleAfterMs = 500);
    void stop();  // blocks until the capture thread joins

private:
    DXGICapture(Microsoft::WRL::ComPtr<ID3D11Device> dev,
                Microsoft::WRL::ComPtr<ID3D11DeviceContext> ctx,
                Microsoft::WRL::ComPtr<IDXGIOutputDuplication> dup)
        : m_device(std::move(dev)), m_ctx(std::move(ctx)), m_dup(std::move(dup)) {}

    void loop(FrameCallback onFrame, std::function<void()> onIdle,
              std::function<void()> onResume, uint32_t idleAfterMs);

    Microsoft::WRL::ComPtr<ID3D11Device>           m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>    m_ctx;
    Microsoft::WRL::ComPtr<IDXGIOutputDuplication> m_dup;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>        m_staging;  // CPU-readable copy

    std::atomic<bool> m_running{false};
    std::thread       m_thread;
};

}  // namespace idsp
