#pragma once
// ---------------------------------------------------------------------------
// mcp_server.h — MCP (Model Context Protocol) server over HTTP
//
// Implements MCP JSON-RPC 2.0 over Streamable HTTP transport:
//   POST /mcp  — JSON-RPC requests (initialize, tools/list, tools/call)
//
// Runs on a background thread. Game operations are delegated to
// GameCommandQueue which executes them on the UI thread.
// ---------------------------------------------------------------------------
#include "game_commands.h"

#include <string>
#include <thread>
#include <atomic>
#include <memory>

namespace x4mcp {

class McpServer {
public:
    explicit McpServer(GameCommandQueue& commands, uint16_t port = 7779,
                       const std::string& bind_address = "0.0.0.0");
    ~McpServer();

    bool start();
    void stop();

private:
    GameCommandQueue& commands_;
    uint16_t port_;
    std::string bind_address_;
    std::thread server_thread_;
    std::atomic<bool> running_{false};

    // Forward declaration — impl in .cpp to avoid httplib in header
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace x4mcp
