// ---------------------------------------------------------------------------
// x4mcp — config.h
// MCP server configuration
// ---------------------------------------------------------------------------
#pragma once

#include <string>
#include <cstdint>

namespace x4mcp {

struct Config {
    uint16_t port = 7779;
    std::string bind_address = "127.0.0.1";

    // Loaded from x4mcp_config.json next to the DLL
    static Config load(const char* extension_path);
};

} // namespace x4mcp
