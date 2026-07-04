# iDisplay — Windows Host

C++ Windows application that turns an iPad/iPhone into a **wireless secondary display** over the local network. It captures the desktop, encodes H.264, streams it to the iOS client over UDP, and injects the iOS touches back as mouse input. Runs as a **tray-only** app (no taskbar window).

Pairs with the [iDisplay iOS client](../ios-client). Connection is **LAN-only**, **iOS-initiated**, and every request must be **approved on this PC** (Accept/Deny prompt).

---

## Features

- Capture primary display (Mirror mode) via DXGI Desktop Duplication.
- H.264 encode via Media Foundation (uses the GPU encoder when available).
- Custom low-overhead UDP video protocol (MTU-safe chunking).
- Touch → mouse injection (`SendInput`).
- **Pairing gate**: only the LAN, only after you press Accept; access is bound to a CSPRNG session token.
- **Idle-stop**: when the desktop is static, streaming pauses to save power; it resumes on the next change.
- Tray app — lives in the notification area ("hidden icons"), no taskbar button.

> Extend mode (a real virtual monitor via Parsec VDD) is stubbed — see [Limitations](#limitations). Mirror mode works today.

---

## Requirements

| Tool | Version |
|------|---------|
| Windows | 10 / 11 |
| MSVC | Visual Studio 2022 Build Tools, C++20 (`cl` 14.4x) |
| Windows SDK | 10.0.22621+ (DXGI 1.5, Media Foundation, Direct3D 11) |
| CMake | 3.28+ |
| vcpkg | any recent; `VCPKG_ROOT` set |

Dependencies (`nlohmann-json`) are declared in `vcpkg.json` and installed automatically on configure — nothing to download by hand.

---

## Build

```bash
# from windows-host/
cmake -B build -G "Visual Studio 17 2022" -A x64 \
  -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake"

# Release
cmake --build build --config Release
# → build/Release/idisplay_host.exe

# Debug
cmake --build build --config Debug
```

The first configure runs `vcpkg install` for the manifest deps (one-time, cached in `build/vcpkg_installed/`).

### Tests

```bash
cmake --build build --config Debug
cd build && ctest -C Debug --output-on-failure
# protocol · ctrl · config  → all pass
```

---

## Run / Deploy

1. Build (or copy) `idisplay_host.exe` to the target PC.
2. Run it. It starts **in the tray** (notification area) — a balloon confirms it's running. There is no main window; right-click the tray icon → **Quit** to exit.
3. Allow it through the firewall on the **private** network the first time Windows asks.
4. On the iOS client, enter this PC's LAN IP and press Connect → an **Accept/Deny** prompt appears here. Press **Accept** to start streaming.

### Ports (open inbound on the LAN)

| Port | Proto | Direction | Purpose |
|------|-------|-----------|---------|
| 7654 | UDP | Windows → iOS | H.264 video |
| 7655 | UDP | iOS → Windows | touch/input |
| 7656 | TCP | bidirectional | control + pairing |

Non-private (non-LAN) source IPs are rejected.

### Configuration

`%APPDATA%\iDisplay\config.json` (created on first save). Schema:

```jsonc
{
  "host":   { "videoPort": 7654, "inputPort": 7655, "ctrlPort": 7656, "bindAddress": "0.0.0.0" },
  "stream": { "defaultMode": "extend", "defaultAspectRatio": "16:9",
              "defaultResolution": { "width": 1920, "height": 1080 },
              "fps": 60, "bitrateKbps": 12000, "keyframeIntervalSec": 2,
              "encoderPreference": "auto" },
  "input":  { "enableTouch": true, "enableScroll": true, "scrollSensitivity": 1.0 }
}
```

All display settings live here on the Windows side; the iOS app only sends basic prefs (FPS, letterbox).

---

## Usage flow

```
iOS: enter host IP → Connect
        │  PAIR_REQUEST (TCP 7656)
        ▼
Windows: tray shows Accept / Deny  ──Deny──▶ connection closed
        │ Accept
        ▼
Windows issues a session token → CONFIG_ACK → starts capture/encode → UDP video (7654)
iOS renders (letterbox, no stretch); taps → UDP input (7655) → mouse on Windows
Static desktop → idle-stop (stream pauses) → activity resumes it automatically
```

---

## Project layout

```
windows-host/
├── CMakeLists.txt          # idisplay_core + idisplay_media libs + idisplay_host exe
├── vcpkg.json              # deps manifest (nlohmann-json)
├── src/
│   ├── main.cpp            # WinMain (tray app entry)
│   ├── app.*               # owns + wires every component
│   ├── config/             # JSON config + %APPDATA% persist
│   ├── display/            # dxgi_capture (mirror), vdd_manager (stub)
│   ├── encoder/            # encoder_factory, mf_encoder, color_convert (BGRA→NV12)
│   ├── input/              # input_injector (SendInput)
│   ├── network/            # protocol, udp_server, input_receiver, ctrl_server, pairing, lan, secure_token
│   └── ui/                 # tray
└── tests/                  # ctest self-tests (protocol, ctrl, config)
```

---

## Status

- ✅ Builds to a tray-only exe; unit tests pass (protocol / pairing gate / config / color convert).
- ⚠️ Full streaming pipeline (capture → encode → net → inject) compiles but needs a real display + GPU + a connected client to verify end-to-end.

## Limitations

- **Extend mode** needs the Parsec VDD SDK in `third_party/parsec-vdd/` (not vendored). Until then the app uses **Mirror** mode.
- **BGRA→NV12** conversion is done on the CPU (`encoder/color_convert.hpp`); move to a GPU/video-processor path for best latency.
- DXGI `ACCESS_LOST` (resolution change / RDP) re-init is a TODO.
- Single iOS client at a time.
