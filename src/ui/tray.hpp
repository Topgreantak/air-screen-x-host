#pragma once
// Win32 system-tray app. README section 5 UI. Owns the notify icon + context menu and the
// R6 Accept/Deny pairing prompt. Runtime-only (needs a desktop session); compile-verified.

#include <functional>
#include <string>

namespace idsp {

class Tray {
public:
    Tray();
    ~Tray();
    Tray(const Tray&) = delete;

    bool install(const std::wstring& tooltip);
    void remove();

    // Blocking Accept/Deny dialog for a pairing request (called from the ctrl thread). R6.
    bool promptAcceptDeny(const std::string& deviceName, const std::string& deviceId);

    std::function<void()> onQuit;

    // Pump the Win32 message loop until Quit is chosen. Call on the main thread.
    void runMessageLoop();

private:
    void showMenu();
    static long long __stdcall wndProc(void* hwnd, unsigned msg, unsigned long long w, long long l);

    void* m_hwnd = nullptr;   // HWND
    bool  m_added = false;
};

}  // namespace idsp
