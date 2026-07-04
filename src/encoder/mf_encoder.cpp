#include "mf_encoder.hpp"
#include <mfapi.h>
#include <mftransform.h>
#include <mferror.h>
#include <codecapi.h>
#include <vector>

// NOTE (runtime-unverified): the H.264 MFT takes NV12 (not BGRA) input. The capture/convert
// stage must hand NV12 to encodeFrame — see wiki [[capture-encode]] TODO for the BGRA→NV12 step.

using Microsoft::WRL::ComPtr;

namespace idsp {

std::unique_ptr<MFEncoder> MFEncoder::create(const EncoderConfig& cfg) {
    auto enc = std::unique_ptr<MFEncoder>(new MFEncoder(cfg));
    if (FAILED(MFStartup(MF_VERSION))) return nullptr;
    if (!enc->init()) return nullptr;
    return enc;
}

MFEncoder::~MFEncoder() {
    if (m_mft && m_streaming) {
        m_mft->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
        m_mft->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
    }
    m_mft.Reset();
    MFShutdown();
}

static ComPtr<IMFMediaType> makeType(GUID major, GUID sub) {
    ComPtr<IMFMediaType> t;
    MFCreateMediaType(&t);
    t->SetGUID(MF_MT_MAJOR_TYPE, major);
    t->SetGUID(MF_MT_SUBTYPE, sub);
    return t;
}

bool MFEncoder::init() {
    // Find a hardware-or-software H.264 encoder MFT.
    MFT_REGISTER_TYPE_INFO out{ MFMediaType_Video, MFVideoFormat_H264 };
    IMFActivate** acts = nullptr; UINT32 count = 0;
    UINT32 flags = MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_ASYNCMFT |
                   MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER;
    if (FAILED(MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER, flags, nullptr, &out, &acts, &count)) ||
        count == 0)
        return false;
    HRESULT hr = acts[0]->ActivateObject(IID_PPV_ARGS(&m_mft));
    for (UINT32 i = 0; i < count; ++i) acts[i]->Release();
    CoTaskMemFree(acts);
    if (FAILED(hr)) return false;

    // Output type: H.264 with target bitrate / size / fps.
    auto outT = makeType(MFMediaType_Video, MFVideoFormat_H264);
    outT->SetUINT32(MF_MT_AVG_BITRATE, m_cfg.bitrateKbps * 1000);
    outT->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    outT->SetUINT32(MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_Main);
    MFSetAttributeSize(outT.Get(), MF_MT_FRAME_SIZE, m_cfg.width, m_cfg.height);
    MFSetAttributeRatio(outT.Get(), MF_MT_FRAME_RATE, m_cfg.fps, 1);
    MFSetAttributeRatio(outT.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    if (FAILED(m_mft->SetOutputType(0, outT.Get(), 0))) return false;

    // Input type: NV12.
    auto inT = makeType(MFMediaType_Video, MFVideoFormat_NV12);
    inT->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    MFSetAttributeSize(inT.Get(), MF_MT_FRAME_SIZE, m_cfg.width, m_cfg.height);
    MFSetAttributeRatio(inT.Get(), MF_MT_FRAME_RATE, m_cfg.fps, 1);
    if (FAILED(m_mft->SetInputType(0, inT.Get(), 0))) return false;

    m_mft->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
    m_mft->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
    m_streaming = true;
    return true;
}

bool MFEncoder::encodeFrame(const uint8_t* nv12, size_t size,
                            uint64_t timestampUs, const EncodedCallback& cb) {
    if (!m_streaming) return false;

    ComPtr<IMFMediaBuffer> buf;
    if (FAILED(MFCreateMemoryBuffer(static_cast<DWORD>(size), &buf))) return false;
    BYTE* dst = nullptr; DWORD maxLen = 0;
    buf->Lock(&dst, &maxLen, nullptr);
    memcpy(dst, nv12, size);
    buf->Unlock();
    buf->SetCurrentLength(static_cast<DWORD>(size));

    ComPtr<IMFSample> sample;
    MFCreateSample(&sample);
    sample->AddBuffer(buf.Get());
    sample->SetSampleTime(static_cast<LONGLONG>(timestampUs * 10));         // 100ns units
    sample->SetSampleDuration(10'000'000 / (m_cfg.fps ? m_cfg.fps : 60));

    HRESULT hr = m_mft->ProcessInput(0, sample.Get(), 0);
    if (hr == MF_E_NOTACCEPTING) { drainOutput(cb); hr = m_mft->ProcessInput(0, sample.Get(), 0); }
    if (FAILED(hr)) return false;

    drainOutput(cb);
    ++m_frameIndex;
    return true;
}

void MFEncoder::drainOutput(const EncodedCallback& cb) {
    MFT_OUTPUT_STREAM_INFO si{};
    m_mft->GetOutputStreamInfo(0, &si);

    for (;;) {
        MFT_OUTPUT_DATA_BUFFER outData{};
        ComPtr<IMFSample> outSample;
        ComPtr<IMFMediaBuffer> outBuf;
        const bool mftAllocates = si.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES;
        if (!mftAllocates) {
            MFCreateSample(&outSample);
            MFCreateMemoryBuffer(si.cbSize ? si.cbSize : (1u << 20), &outBuf);
            outSample->AddBuffer(outBuf.Get());
            outData.pSample = outSample.Get();
        }

        DWORD status = 0;
        HRESULT hr = m_mft->ProcessOutput(0, 1, &outData, &status);
        if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) break;
        if (FAILED(hr)) break;

        ComPtr<IMFSample> produced = outData.pSample;  // MFT-allocated case owns it here
        if (produced) {
            ComPtr<IMFMediaBuffer> mb;
            produced->ConvertToContiguousBuffer(&mb);
            BYTE* p = nullptr; DWORD len = 0;
            mb->Lock(&p, nullptr, &len);
            UINT32 key = 0;
            produced->GetUINT32(MFSampleExtension_CleanPoint, &key);
            LONGLONG t = 0; produced->GetSampleTime(&t);
            if (cb && len) cb(p, len, key != 0, static_cast<uint64_t>(t / 10));
            mb->Unlock();
        }
        if (mftAllocates && outData.pSample) outData.pSample->Release();
        if (outData.pEvents) outData.pEvents->Release();
    }
}

}  // namespace idsp
