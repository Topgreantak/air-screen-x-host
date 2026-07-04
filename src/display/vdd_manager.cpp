#include "vdd_manager.hpp"

namespace idsp {

// ponytail: no Parsec VDD SDK vendored → Extend mode unavailable; app uses Mirror.
// Replace this stub with a real implementation once third_party/parsec-vdd/ exists.
std::optional<VddManager> VddManager::create(const DisplayConfig&) {
    return std::nullopt;
}

VddManager::~VddManager() = default;
VddManager::VddManager(VddManager&& o) noexcept : m_displayIndex(o.m_displayIndex) {
    o.m_displayIndex = -1;
}
VddManager& VddManager::operator=(VddManager&& o) noexcept {
    if (this != &o) { m_displayIndex = o.m_displayIndex; o.m_displayIndex = -1; }
    return *this;
}
bool VddManager::updateResolution(const DisplayConfig&) { return false; }

}  // namespace idsp
