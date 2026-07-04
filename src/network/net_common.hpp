#pragma once
// Winsock init (RAII, ref-counted) shared by the UDP/TCP servers.
#include <winsock2.h>
#include <ws2tcpip.h>

namespace idsp {

// Bump/drop the global Winsock refcount. Construct one per socket-owning object.
class WsaGuard {
public:
    WsaGuard()  { WSADATA d; WSAStartup(MAKEWORD(2, 2), &d); }
    ~WsaGuard() { WSACleanup(); }
    WsaGuard(const WsaGuard&) = delete;
    WsaGuard& operator=(const WsaGuard&) = delete;
};

}  // namespace idsp
