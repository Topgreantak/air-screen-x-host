#pragma once
// LAN-only gate (R5). Pure IPv4 helpers → unit-testable, no sockets.
// The ctrl/UDP servers must reject any client that isn't on a private LAN
// (and, when a host IP+mask is known, not on the same subnet).

#include <cstdint>
#include <optional>
#include <string>

namespace idsp::lan {

// Parse dotted-quad "a.b.c.d" → host-order uint32. nullopt on malformed input (trust boundary).
inline std::optional<uint32_t> parseIPv4(const std::string& s) {
    uint32_t parts[4]; int n = 0; uint32_t cur = 0; int digits = 0;
    for (char c : s) {
        if (c == '.') {
            if (digits == 0 || n >= 3) return std::nullopt;
            parts[n++] = cur; cur = 0; digits = 0;
        } else if (c >= '0' && c <= '9') {
            cur = cur * 10 + uint32_t(c - '0');
            if (++digits > 3 || cur > 255) return std::nullopt;
        } else {
            return std::nullopt;
        }
    }
    if (digits == 0 || n != 3) return std::nullopt;
    parts[3] = cur;
    return (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];
}

// RFC1918 private + loopback + link-local. These are the only IPs allowed to connect (R5).
inline bool isPrivateIPv4(uint32_t ip) {
    if ((ip & 0xFF000000u) == 0x0A000000u) return true;  // 10.0.0.0/8
    if ((ip & 0xFFF00000u) == 0xAC100000u) return true;  // 172.16.0.0/12
    if ((ip & 0xFFFF0000u) == 0xC0A80000u) return true;  // 192.168.0.0/16
    if ((ip & 0xFFFF0000u) == 0xA9FE0000u) return true;  // 169.254.0.0/16 link-local
    if ((ip & 0xFF000000u) == 0x7F000000u) return true;  // 127.0.0.0/8 loopback
    return false;
}

// Same subnet as host under mask (both host-order). Prefer this when the host interface is known.
inline bool sameSubnet(uint32_t ip, uint32_t hostIp, uint32_t mask) {
    return (ip & mask) == (hostIp & mask);
}

}  // namespace idsp::lan
