#pragma once
// Parsec Virtual Display (Extend mode). README section 5.3.
// STUB: the Parsec VDD SDK header (third_party/parsec-vdd/parsec-vdd.h) is NOT vendored yet,
// so create() returns nullopt → Extend mode is unavailable and the app falls back to Mirror.
// When the SDK is added, implement against it and keep the RAII destroy guarantee.
// See wiki [[capture-encode]] TODO.

#include <cstdint>
#include <optional>

namespace idsp {

class VddManager {
public:
    struct DisplayConfig { uint32_t width, height, refreshRate; };

    // Returns nullopt until the Parsec VDD SDK is integrated.
    static std::optional<VddManager> create(const DisplayConfig& cfg);

    ~VddManager();
    VddManager(const VddManager&) = delete;
    VddManager& operator=(const VddManager&) = delete;
    VddManager(VddManager&&) noexcept;
    VddManager& operator=(VddManager&&) noexcept;

    bool updateResolution(const DisplayConfig& cfg);
    int  displayIndex() const { return m_displayIndex; }

private:
    VddManager() = default;
    int m_displayIndex = -1;
};

}  // namespace idsp
