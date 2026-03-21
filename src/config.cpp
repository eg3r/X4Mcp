// ---------------------------------------------------------------------------
// x4mcp — config.cpp
// ---------------------------------------------------------------------------
#include "config.h"
#include <x4native.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <string>

namespace x4mcp {

Config Config::load(const char* extension_path) {
    Config cfg;

    std::string path = std::string(extension_path);
    if (!path.empty() && (path.back() == '\\' || path.back() == '/'))
        path += "x4mcp_config.json";
    else
        path += "\\x4mcp_config.json";

    std::ifstream file(path);
    if (!file.is_open()) {
        x4n::log::warn(
            ("x4mcp: config file not found at " + path + " — using defaults").c_str());
        return cfg;
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    auto j = nlohmann::json::parse(content, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded()) {
        x4n::log::warn(("x4mcp: failed to parse config JSON at " + path + " — using defaults").c_str());
        return cfg;
    }

    if (j.contains("port") && j["port"].is_number()) {
        int p = j["port"].get<int>();
        if (p > 0 && p <= 65535) cfg.port = static_cast<uint16_t>(p);
    }

    if (j.contains("bind_address") && j["bind_address"].is_string()) {
        cfg.bind_address = j["bind_address"].get<std::string>();
    }

    x4n::log::info("x4mcp: config loaded — bind=%s port=%d",
                   cfg.bind_address.c_str(), cfg.port);

    return cfg;
}

} // namespace x4mcp
