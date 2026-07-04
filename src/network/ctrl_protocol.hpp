#pragma once
// Control-channel messages (port 7656, TCP, newline-delimited JSON). See README section 4.3.
// Pure parse/build via nlohmann-json — no Win32/socket deps → unit-testable.

#include <cstdint>
#include <optional>
#include <string>
#include <nlohmann/json.hpp>

namespace idsp::ctrl {

enum class Type {
    PairRequest, PairAccept, PairDeny, SetPrefs,
    ConfigAck, ConfigUpdate, StreamIdle, StreamResume,
    Ping, Pong, Disconnect, Unknown
};

inline const char* toString(Type t) {
    switch (t) {
        case Type::PairRequest:  return "PAIR_REQUEST";
        case Type::PairAccept:   return "PAIR_ACCEPT";
        case Type::PairDeny:     return "PAIR_DENY";
        case Type::SetPrefs:     return "SET_PREFS";
        case Type::ConfigAck:    return "CONFIG_ACK";
        case Type::ConfigUpdate: return "CONFIG_UPDATE";
        case Type::StreamIdle:   return "STREAM_IDLE";
        case Type::StreamResume: return "STREAM_RESUME";
        case Type::Ping:         return "PING";
        case Type::Pong:         return "PONG";
        case Type::Disconnect:   return "DISCONNECT";
        default:                 return "UNKNOWN";
    }
}

inline Type typeFromString(const std::string& s) {
    if (s == "PAIR_REQUEST")  return Type::PairRequest;
    if (s == "PAIR_ACCEPT")   return Type::PairAccept;
    if (s == "PAIR_DENY")     return Type::PairDeny;
    if (s == "SET_PREFS")     return Type::SetPrefs;
    if (s == "CONFIG_ACK")    return Type::ConfigAck;
    if (s == "CONFIG_UPDATE") return Type::ConfigUpdate;
    if (s == "STREAM_IDLE")   return Type::StreamIdle;
    if (s == "STREAM_RESUME") return Type::StreamResume;
    if (s == "PING")          return Type::Ping;
    if (s == "PONG")          return Type::Pong;
    if (s == "DISCONNECT")    return Type::Disconnect;
    return Type::Unknown;
}

// ---- inbound (iOS → Windows) parsed payloads ----
struct PairRequest { std::string deviceName, deviceId; };
struct SetPrefs    { uint32_t fps = 0; bool letterbox = true; };

// A safely-parsed message. Returns nullopt on invalid JSON.
// `type` is Unknown if the "type" field is missing/unrecognized — never throws.
struct Message {
    Type                       type = Type::Unknown;
    std::optional<std::string> token;   // sessionToken if present
};

inline std::optional<Message> parse(const std::string& line) {
    nlohmann::json j = nlohmann::json::parse(line, /*cb=*/nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded() || !j.is_object()) return std::nullopt;  // trust boundary — R2
    Message m;
    if (auto it = j.find("type"); it != j.end() && it->is_string())
        m.type = typeFromString(it->get<std::string>());
    if (auto it = j.find("sessionToken"); it != j.end() && it->is_string())
        m.token = it->get<std::string>();
    return m;
}

// Typed extractors — return nullopt if fields missing/wrong-typed.
inline std::optional<PairRequest> asPairRequest(const std::string& line) {
    nlohmann::json j = nlohmann::json::parse(line, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return std::nullopt;
    if (!j.value("type", "").empty() && j["type"] != "PAIR_REQUEST") return std::nullopt;
    if (!j.contains("deviceId") || !j["deviceId"].is_string()) return std::nullopt;
    return PairRequest{ j.value("deviceName", ""), j["deviceId"].get<std::string>() };
}

inline std::optional<SetPrefs> asSetPrefs(const std::string& line) {
    nlohmann::json j = nlohmann::json::parse(line, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return std::nullopt;
    SetPrefs p;
    if (auto it = j.find("fps"); it != j.end() && it->is_number_unsigned())
        p.fps = it->get<uint32_t>();
    if (auto d = j.find("display"); d != j.end() && d->is_object())
        p.letterbox = d->value("letterbox", true);
    return p;
}

// ---- outbound (Windows → iOS) builders. Each returns a single newline-terminated line. ----
inline std::string buildPairAccept(const std::string& token) {
    return nlohmann::json{{"type", "PAIR_ACCEPT"}, {"sessionToken", token}}.dump() + "\n";
}
inline std::string buildPairDeny(const std::string& reason) {
    return nlohmann::json{{"type", "PAIR_DENY"}, {"reason", reason}}.dump() + "\n";
}
inline std::string buildConfigAck(const std::string& token, const std::string& mode,
                                  const std::string& aspect, uint32_t w, uint32_t h,
                                  uint32_t fps, const std::string& codec = "h264") {
    return nlohmann::json{{"type", "CONFIG_ACK"}, {"sessionToken", token},
                          {"mode", mode}, {"aspectRatio", aspect},
                          {"virtualWidth", w}, {"virtualHeight", h},
                          {"fps", fps}, {"codec", codec}}.dump() + "\n";
}
inline std::string buildStreamState(const std::string& token, bool idle) {
    return nlohmann::json{{"type", idle ? "STREAM_IDLE" : "STREAM_RESUME"},
                          {"sessionToken", token}}.dump() + "\n";
}
inline std::string buildPing(const std::string& token, uint64_t ts) {
    return nlohmann::json{{"type", "PING"}, {"sessionToken", token}, {"ts", ts}}.dump() + "\n";
}

}  // namespace idsp::ctrl
