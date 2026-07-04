#pragma once
// Session-token generator for the R6 pairing gate.
// Uses Windows CNG (BCryptGenRandom) — a real CSPRNG, not std::mt19937.
// ponytail: BCrypt is in-box (bcrypt.lib), no extra dep; don't hand-roll or downgrade the RNG —
// this token is the access gate.
#include <string>
#include <cstddef>

namespace idsp {

// Returns a hex string of `bytes`*2 chars from the OS CSPRNG. Empty string on failure.
std::string secureToken(size_t bytes = 16);

}  // namespace idsp
