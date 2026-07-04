#include "dxgi_capture.hpp"
#include <chrono>

using Microsoft::WRL::ComPtr;

namespace idsp {

static uint64_t nowUs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());
}

std::optional<DXGICapture> DXGICapture::create(int outputIndex) {
    ComPtr<ID3D11Device> dev; ComPtr<ID3D11DeviceContext> ctx;
    D3D_FEATURE_LEVEL fl{};
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                                 nullptr, 0, D3D11_SDK_VERSION, &dev, &fl, &ctx)))
        return std::nullopt;

    ComPtr<IDXGIDevice> dxgiDev;
    if (FAILED(dev.As(&dxgiDev))) return std::nullopt;
    ComPtr<IDXGIAdapter> adapter;
    if (FAILED(dxgiDev->GetAdapter(&adapter))) return std::nullopt;
    ComPtr<IDXGIOutput> output;
    if (FAILED(adapter->EnumOutputs(static_cast<UINT>(outputIndex), &output))) return std::nullopt;
    ComPtr<IDXGIOutput1> output1;
    if (FAILED(output.As(&output1))) return std::nullopt;

    ComPtr<IDXGIOutputDuplication> dup;
    if (FAILED(output1->DuplicateOutput(dev.Get(), &dup))) return std::nullopt;

    return DXGICapture(std::move(dev), std::move(ctx), std::move(dup));
}

DXGICapture::DXGICapture(DXGICapture&& o) noexcept
    : m_device(std::move(o.m_device)), m_ctx(std::move(o.m_ctx)), m_dup(std::move(o.m_dup)),
      m_staging(std::move(o.m_staging)) {}

DXGICapture::~DXGICapture() { stop(); }

bool DXGICapture::start(FrameCallback onFrame, std::function<void()> onIdle,
                        std::function<void()> onResume, uint32_t idleAfterMs) {
    if (!m_dup) return false;
    m_running.store(true, std::memory_order_release);
    m_thread = std::thread(&DXGICapture::loop, this, std::move(onFrame),
                           std::move(onIdle), std::move(onResume), idleAfterMs);
    return true;
}

void DXGICapture::stop() {
    m_running.store(false, std::memory_order_release);
    if (m_thread.joinable()) m_thread.join();
}

void DXGICapture::loop(FrameCallback onFrame, std::function<void()> onIdle,
                       std::function<void()> onResume, uint32_t idleAfterMs) {
    bool     idle          = false;
    uint64_t lastFrameUs   = nowUs();

    while (m_running.load(std::memory_order_acquire)) {
        ComPtr<IDXGIResource> res;
        DXGI_OUTDUPL_FRAME_INFO info{};
        HRESULT hr = m_dup->AcquireNextFrame(16, &info, &res);

        if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
            // No new frame. R4 idle-stop: after idleAfterMs of stasis, signal idle once.
            if (idleAfterMs && !idle && nowUs() - lastFrameUs >= idleAfterMs * 1000ull) {
                idle = true;
                if (onIdle) onIdle();
            }
            continue;
        }
        if (FAILED(hr)) break;  // TODO: reinit on DXGI_ERROR_ACCESS_LOST

        // A real frame arrived. AccumulatedFrames==0 with no dirty region → treat as no-op.
        const bool changed = info.AccumulatedFrames > 0 || info.TotalMetadataBufferSize > 0;
        if (changed) {
            if (idle) { idle = false; if (onResume) onResume(); }
            lastFrameUs = nowUs();

            ComPtr<ID3D11Texture2D> tex;
            if (SUCCEEDED(res.As(&tex))) {
                D3D11_TEXTURE2D_DESC desc{}; tex->GetDesc(&desc);
                // (Re)create a CPU-readable staging texture matching the frame.
                if (!m_staging) {
                    D3D11_TEXTURE2D_DESC sd = desc;
                    sd.Usage = D3D11_USAGE_STAGING;
                    sd.BindFlags = 0;
                    sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                    sd.MiscFlags = 0;
                    m_device->CreateTexture2D(&sd, nullptr, &m_staging);
                }
                if (m_staging) {
                    m_ctx->CopyResource(m_staging.Get(), tex.Get());
                    D3D11_MAPPED_SUBRESOURCE map{};
                    if (SUCCEEDED(m_ctx->Map(m_staging.Get(), 0, D3D11_MAP_READ, 0, &map))) {
                        if (onFrame)
                            onFrame(static_cast<const uint8_t*>(map.pData),
                                    desc.Width, desc.Height, map.RowPitch, nowUs());
                        m_ctx->Unmap(m_staging.Get(), 0);
                    }
                }
            }
        }
        m_dup->ReleaseFrame();  // paired with every successful AcquireNextFrame
    }
}

}  // namespace idsp
