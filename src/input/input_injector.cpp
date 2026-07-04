#include "input_injector.hpp"
#include <windows.h>

namespace idsp {

void InputInjector::injectTouch(const TouchEvent& e) const {
    const AbsPoint p = normalizedToAbsolute(e.normX, e.normY);

    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dx = p.x;
    in.mi.dy = p.y;
    in.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
    if (m_virtualDesktop) in.mi.dwFlags |= MOUSEEVENTF_VIRTUALDESK;

    switch (e.type) {
        case TouchEvent::Type::Down:   in.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN; break;
        case TouchEvent::Type::Up:     in.mi.dwFlags |= MOUSEEVENTF_LEFTUP;   break;
        case TouchEvent::Type::Move:   /* move only (drag if button already held) */ break;
        case TouchEvent::Type::Scroll:
            in.mi.dwFlags = MOUSEEVENTF_WHEEL;
            in.mi.mouseData = static_cast<DWORD>(WHEEL_DELTA);  // one notch; sign/magnitude TODO
            break;
    }
    SendInput(1, &in, sizeof(INPUT));
}

}  // namespace idsp
