#pragma once
// App config (README section 9 schema). JSON serde via nlohmann. Persisted at
// %APPDATA%\iDisplay\config.json. Parse is pure/testable; load/save touch the filesystem.

#include <cstdint>
#include <string>

namespace idsp {

enum class Mode { Mirror, Extend };

inline const char* modeName(Mode m) { return m == Mode::Mirror ? "mirror" : "extend"; }
inline Mode modeFromString(const std::string& s) { return s == "mirror" ? Mode::Mirror : Mode::Extend; }

struct Resolution { uint32_t width = 1920, height = 1080; };

struct StreamConfig {
    Mode        mode                = Mode::Extend;
    std::string aspectRatio         = "16:9";       // native|16:9|4:3|3:2|1:1
    Resolution  resolution;
    uint32_t    fps                 = 60;
    uint32_t    bitrateKbps         = 12000;
    uint32_t    keyframeIntervalSec = 2;
    std::string encoderPreference   = "auto";       // auto|nvenc|qsv|amf|mf
};

struct HostConfig {
    uint16_t    videoPort   = 7654;
    uint16_t    inputPort   = 7655;
    uint16_t    ctrlPort    = 7656;
    std::string bindAddress = "0.0.0.0";
};

struct InputConfig {
    bool  enableTouch       = true;
    bool  enableScroll      = true;
    float scrollSensitivity = 1.0f;
};

struct Config {
    HostConfig   host;
    StreamConfig stream;
    InputConfig  input;
};

// Pure: parse from JSON text. On malformed input, `ok=false` and defaults are returned
// (trust boundary — never throws).
Config      configFromJson(const std::string& text, bool& ok);
std::string configToJson(const Config& c);

// Filesystem: path is %APPDATA%\iDisplay\config.json. loadConfig returns defaults if absent.
std::string configPath();
Config      loadConfig();
bool        saveConfig(const Config& c);

}  // namespace idsp
