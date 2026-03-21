// ---------------------------------------------------------------------------
// x4mcp — main.cpp
// Extension entry point
//
// Loads an MCP (Model Context Protocol) server inside the X4 game process.
// External tools (Claude Code, etc.) can connect and control the game:
//   - Load savegames
//   - Check if game is loaded
//   - Shut down the game
//   - Execute chat/debug commands
// ---------------------------------------------------------------------------
#include <x4native.h>

#include "mcp_server.h"
#include "game_commands.h"
#include "config.h"

#include <memory>
#include <string>

static x4mcp::GameCommandQueue g_commands;
static std::unique_ptr<x4mcp::McpServer> g_server;
static x4mcp::Config g_config;
static std::string g_extension_path;
static bool g_game_loaded = false;

static constexpr const char* STASH_GAME_LOADED = "game_loaded";

static void persist_game_loaded(bool loaded) {
    g_game_loaded = loaded;
    x4n::stash::set(STASH_GAME_LOADED, uint8_t(loaded ? 1 : 0));
}

static bool restore_game_loaded() {
    uint8_t v = 0;
    if (x4n::stash::get(STASH_GAME_LOADED, &v) && v) {
        g_game_loaded = true;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Lua result event bridges
// When Lua executes a game command, it reports results back via events.
// We forward these to the command queue to unblock the waiting HTTP thread.
// ---------------------------------------------------------------------------
static void on_lua_result(const char* event_name) {
    // event_name is the raw string param from the Lua event
    // The C++ event name is what we subscribed to (e.g. "x4mcp_load_game_result")
    // But we need the Lua event name for matching. We register the bridge
    // so Lua event "x4mcp.load_game_result" -> C++ event "x4mcp_load_game_result".
    // The param contains the result string.
}

// Generic result handler factory
static void on_load_game_result(const char* param) {
    g_commands.on_lua_result("x4mcp.load_game_result", param ? param : "");
}

static void on_quit_game_result(const char* param) {
    g_commands.on_lua_result("x4mcp.quit_game_result", param ? param : "");
}

static void on_chat_command_result(const char* param) {
    g_commands.on_lua_result("x4mcp.chat_command_result", param ? param : "");
}

static void on_list_saves_result(const char* param) {
    g_commands.on_lua_result("x4mcp.list_saves_result", param ? param : "");
}

// ---------------------------------------------------------------------------
// Frame update — process queued commands on the UI thread
// ---------------------------------------------------------------------------
static void on_frame_update() {
    g_commands.process_pending();
}

// ---------------------------------------------------------------------------
// Game loaded — start MCP server
// ---------------------------------------------------------------------------
static void on_game_loaded() {
    if (g_game_loaded) return;
    persist_game_loaded(true);
    x4n::log::info("Game loaded — MCP server already running");
}

// ---------------------------------------------------------------------------
// Extension entry point
// ---------------------------------------------------------------------------
X4N_EXTENSION {
    x4n::log::info("X4MCP extension loaded (v0.1.0)");

    // Load config from x4mcp_config.json next to the DLL
    g_extension_path = x4n::path();
    g_config = x4mcp::Config::load(g_extension_path.c_str());

    // Restore game_loaded flag from stash (survives /reloadui)
    restore_game_loaded();

    // Start MCP server immediately so clients can connect at the main menu
    g_server = std::make_unique<x4mcp::McpServer>(g_commands, g_config.port, g_config.bind_address);
    g_server->start();

    // Register Lua event bridges (Lua event -> C++ event)
    x4n::bridge_lua_event("x4mcp.load_game_result",    "x4mcp_load_game_result");
    x4n::bridge_lua_event("x4mcp.quit_game_result",    "x4mcp_quit_game_result");
    x4n::bridge_lua_event("x4mcp.chat_command_result",  "x4mcp_chat_command_result");
    x4n::bridge_lua_event("x4mcp.list_saves_result",    "x4mcp_list_saves_result");

    // Subscribe to the bridged C++ events
    x4n::on("x4mcp_load_game_result",    on_load_game_result);
    x4n::on("x4mcp_quit_game_result",    on_quit_game_result);
    x4n::on("x4mcp_chat_command_result",  on_chat_command_result);
    x4n::on("x4mcp_list_saves_result",    on_list_saves_result);

    // Lifecycle events
    x4n::on("on_game_loaded",  on_game_loaded);
    x4n::on("on_frame_update", on_frame_update);
}

X4N_SHUTDOWN {
    x4n::log::info("X4MCP shutting down");
    if (g_server) {
        g_server->stop();
        g_server.reset();
    }
}
