// iDisplay Windows host entry point. Tray app (WIN32 subsystem).
#include <windows.h>
#include "app.hpp"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    idsp::App app;
    return app.run();
}
