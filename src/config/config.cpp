#include "config.hpp"
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace idsp {

using nlohmann::json;

Config configFromJson(const std::string& text, bool& ok) {
    Config c;  // defaults
    json j = json::parse(text, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded() || !j.is_object()) { ok = false; return c; }
    ok = true;

    if (auto h = j.find("host"); h != j.end() && h->is_object()) {
        c.host.videoPort   = h->value("videoPort",   c.host.videoPort);
        c.host.inputPort   = h->value("inputPort",   c.host.inputPort);
        c.host.ctrlPort    = h->value("ctrlPort",    c.host.ctrlPort);
        c.host.bindAddress = h->value("bindAddress", c.host.bindAddress);
    }
    if (auto s = j.find("stream"); s != j.end() && s->is_object()) {
        c.stream.mode                = modeFromString(s->value("defaultMode", modeName(c.stream.mode)));
        c.stream.aspectRatio         = s->value("defaultAspectRatio", c.stream.aspectRatio);
        if (auto r = s->find("defaultResolution"); r != s->end() && r->is_object()) {
            c.stream.resolution.width  = r->value("width",  c.stream.resolution.width);
            c.stream.resolution.height = r->value("height", c.stream.resolution.height);
        }
        c.stream.fps                 = s->value("fps",                 c.stream.fps);
        c.stream.bitrateKbps         = s->value("bitrateKbps",         c.stream.bitrateKbps);
        c.stream.keyframeIntervalSec = s->value("keyframeIntervalSec", c.stream.keyframeIntervalSec);
        c.stream.encoderPreference   = s->value("encoderPreference",   c.stream.encoderPreference);
    }
    if (auto i = j.find("input"); i != j.end() && i->is_object()) {
        c.input.enableTouch       = i->value("enableTouch",       c.input.enableTouch);
        c.input.enableScroll      = i->value("enableScroll",      c.input.enableScroll);
        c.input.scrollSensitivity = i->value("scrollSensitivity", c.input.scrollSensitivity);
    }
    return c;
}

std::string configToJson(const Config& c) {
    json j = {
        {"host", {
            {"videoPort", c.host.videoPort}, {"inputPort", c.host.inputPort},
            {"ctrlPort", c.host.ctrlPort}, {"bindAddress", c.host.bindAddress}}},
        {"stream", {
            {"defaultMode", modeName(c.stream.mode)},
            {"defaultAspectRatio", c.stream.aspectRatio},
            {"defaultResolution", {{"width", c.stream.resolution.width},
                                   {"height", c.stream.resolution.height}}},
            {"fps", c.stream.fps}, {"bitrateKbps", c.stream.bitrateKbps},
            {"keyframeIntervalSec", c.stream.keyframeIntervalSec},
            {"encoderPreference", c.stream.encoderPreference}}},
        {"input", {
            {"enableTouch", c.input.enableTouch}, {"enableScroll", c.input.enableScroll},
            {"scrollSensitivity", c.input.scrollSensitivity}}},
    };
    return j.dump(2);
}

std::string configPath() {
    const char* appdata = std::getenv("APPDATA");
    std::filesystem::path base = appdata ? appdata : ".";
    return (base / "iDisplay" / "config.json").string();
}

Config loadConfig() {
    std::ifstream f(configPath());
    if (!f) return Config{};  // defaults
    std::stringstream ss; ss << f.rdbuf();
    bool ok = false;
    return configFromJson(ss.str(), ok);  // defaults on parse failure
}

bool saveConfig(const Config& c) {
    std::filesystem::path p = configPath();
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);
    std::ofstream f(p, std::ios::trunc);
    if (!f) return false;
    f << configToJson(c);
    return f.good();
}

}  // namespace idsp
