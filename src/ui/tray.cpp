#include "tray.hpp"
#include <windows.h>
#include <shellapi.h>

namespace idsp {

static constexpr UINT WM_TRAY = WM_APP + 1;
static constexpr UINT ID_QUIT = 1001;
static constexpr wchar_t kClass[] = L"iDisplayTrayWnd";

Tray::Tray() = default;
Tray::~Tray() { remove(); }

LRESULT CALLBACK realWndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    auto* self = reinterpret_cast<Tray*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_TRAY:
            if (LOWORD(l) == WM_RBUTTONUP || LOWORD(l) == WM_CONTEXTMENU) {
                POINT pt; GetCursorPos(&pt);
                HMENU menu = CreatePopupMenu();
                AppendMenuW(menu, MF_STRING, ID_QUIT, L"Quit iDisplay");
                SetForegroundWindow(hwnd);
                TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
                DestroyMenu(menu);
            }
            return 0;
        case WM_COMMAND:
            if (LOWORD(w) == ID_QUIT) {
                if (self && self->onQuit) self->onQuit();
                PostQuitMessage(0);
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, w, l);
}

bool Tray::install(const std::wstring& tooltip) {
    HINSTANCE inst = GetModuleHandle(nullptr);
    WNDCLASSW wc{};
    wc.lpfnWndProc   = realWndProc;
    wc.hInstance     = inst;
    wc.lpszClassName = kClass;
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(0, kClass, L"iDisplay", 0, 0, 0, 0, 0,
                                HWND_MESSAGE, nullptr, inst, nullptr);
    if (!hwnd) return false;
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    m_hwnd = hwnd;

    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd   = hwnd;
    nid.uID    = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAY;
    nid.hIcon  = LoadIcon(nullptr, IDI_APPLICATION);
    lstrcpynW(nid.szTip, tooltip.c_str(), 128);
    if (!Shell_NotifyIconW(NIM_ADD, &nid)) return false;
    m_added = true;

    // One-shot balloon so the user knows it's running in the (hidden-icons) tray.
    nid.uFlags = NIF_INFO;
    lstrcpynW(nid.szInfoTitle, L"iDisplay", 64);
    lstrcpynW(nid.szInfo, L"Running in the tray. Right-click the icon to quit.", 256);
    nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &nid);
    return true;
}

void Tray::remove() {
    if (m_added && m_hwnd) {
        NOTIFYICONDATAW nid{};
        nid.cbSize = sizeof(nid);
        nid.hWnd = static_cast<HWND>(m_hwnd);
        nid.uID  = 1;
        Shell_NotifyIconW(NIM_DELETE, &nid);
        m_added = false;
    }
    if (m_hwnd) { DestroyWindow(static_cast<HWND>(m_hwnd)); m_hwnd = nullptr; }
}

bool Tray::promptAcceptDeny(const std::string& deviceName, const std::string& deviceId) {
    std::string body = "Allow \"" + (deviceName.empty() ? deviceId : deviceName) +
                       "\" (" + deviceId + ") to connect as a secondary display?";
    int r = MessageBoxA(nullptr, body.c_str(), "iDisplay — connection request",
                        MB_YESNO | MB_ICONQUESTION | MB_SYSTEMMODAL | MB_SETFOREGROUND);
    return r == IDYES;  // deny-by-default on anything but Yes (R6)
}

void Tray::runMessageLoop() {
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

// (declared member kept for the header's signature; real proc is realWndProc above)
long long __stdcall Tray::wndProc(void*, unsigned, unsigned long long, long long) { return 0; }

}  // namespace idsp
