#include "secure_token.hpp"
#include <vector>
#include <windows.h>
#include <bcrypt.h>

namespace idsp {

std::string secureToken(size_t bytes) {
    std::vector<unsigned char> buf(bytes);
    NTSTATUS st = BCryptGenRandom(
        nullptr, buf.data(), static_cast<ULONG>(buf.size()),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (st != 0 /*STATUS_SUCCESS*/) return {};

    static const char hex[] = "0123456789abcdef";
    std::string out;
    out.reserve(bytes * 2);
    for (unsigned char b : buf) {
        out.push_back(hex[b >> 4]);
        out.push_back(hex[b & 0x0F]);
    }
    return out;
}

}  // namespace idsp
