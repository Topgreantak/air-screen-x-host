#pragma once
// Pairing state machine — the R6 access gate. Deny-by-default.
// Pure logic: token generator + clock are injected → unit-testable without Win32/sockets.
// Single client at a time. ponytail: one active session; multi-client needs a token->session
// map + per-device state — add only if the product allows several iOS clients at once.

#include <cstdint>
#include <functional>
#include <string>
#include <utility>

namespace idsp {

enum class PairState { Idle, AwaitingApproval, Paired };

class PairingManager {
public:
    using TokenGen = std::function<std::string()>;
    using ClockMs  = std::function<uint64_t()>;  // monotonic ms

    PairingManager(uint64_t promptTimeoutMs, TokenGen gen, ClockMs clock)
        : m_timeoutMs(promptTimeoutMs), m_gen(std::move(gen)), m_clock(std::move(clock)) {}

    PairState state() const { return m_state; }

    // iOS sent PAIR_REQUEST. Returns true if a fresh prompt should be raised.
    // Rejects (returns false) if already awaiting or paired — one client at a time.
    bool onRequest(const std::string& deviceId) {
        expireIfNeeded();
        if (m_state != PairState::Idle) return false;
        m_state       = PairState::AwaitingApproval;
        m_deviceId    = deviceId;
        m_requestedAt = m_clock();
        return true;
    }

    // User pressed Accept on the tray prompt. Issues + returns the session token.
    // No-op returning empty if not awaiting (e.g. prompt already timed out).
    std::string accept() {
        expireIfNeeded();
        if (m_state != PairState::AwaitingApproval) return {};
        m_token = m_gen();
        m_state = PairState::Paired;
        return m_token;
    }

    // User pressed Deny, or an explicit teardown. Back to Idle.
    void deny() {
        m_state = PairState::Idle;
        m_token.clear();
        m_deviceId.clear();
    }

    // Client disconnected / DISCONNECT received.
    void reset() { deny(); }

    // Validate a token carried on an inbound message. Deny-by-default:
    // true ONLY when Paired and the token matches exactly (constant work, no early length leak
    // concern here — LAN + user-approved, not a remote brute-force surface).
    bool validate(const std::string& token) {
        expireIfNeeded();
        return m_state == PairState::Paired && !m_token.empty() && token == m_token;
    }

    // Call periodically (or before handling input). Returns true if a pending prompt just expired.
    bool expireIfNeeded() {
        if (m_state == PairState::AwaitingApproval &&
            m_clock() - m_requestedAt >= m_timeoutMs) {
            m_state = PairState::Idle;
            m_deviceId.clear();
            return true;
        }
        return false;
    }

    const std::string& pendingDeviceId() const { return m_deviceId; }

private:
    uint64_t    m_timeoutMs;
    TokenGen    m_gen;
    ClockMs     m_clock;
    PairState   m_state = PairState::Idle;
    std::string m_token;
    std::string m_deviceId;
    uint64_t    m_requestedAt = 0;
};

}  // namespace idsp
