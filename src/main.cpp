// ---------------------------------------------------------------------------
// x4mcp — main.cpp
// Extension entry point
//
// Loads an MCP (Model Context Protocol) server inside the X4 game process.
// External tools (Claude Code, etc.) can connect and control the game:
//   - Load savegames
//   - Check if game is started
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
static bool g_game_started = false;

static constexpr const char* STASH_GAME_STARTED = "game_started";

static void persist_game_started(bool loaded) {
    g_game_started = loaded;
    x4n::stash::set(STASH_GAME_STARTED, uint8_t(loaded ? 1 : 0));
}

static bool restore_game_started() {
    uint8_t v = 0;
    if (x4n::stash::get(STASH_GAME_STARTED, &v) && v) {
        g_game_started = true;
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
static int g_frame_counter = 0;

static void on_frame_update() {
    // Check game state every ~60 frames
    if (g_game_started && ++g_frame_counter >= 60) {
        g_frame_counter = 0;
        auto* game = x4n::game();
        if (!game || !game->GetPlayerID || game->GetPlayerID() == 0) {
            persist_game_started(false);
            g_commands.set_game_started(false);
            x4n::log::info("Player left game — game_started reset");
        }
    }
    g_commands.process_pending();
}

// ---------------------------------------------------------------------------
// Universe ready — world fully constructed (all stations built, both new game and save load)
// ---------------------------------------------------------------------------
static void on_game_started() {
    if (g_game_started) return;
    persist_game_started(true);
    g_commands.set_game_started(true);
    x4n::log::info("Game started — MCP server already running");
}

// ---------------------------------------------------------------------------
// Extension entry point
// ---------------------------------------------------------------------------
X4N_EXTENSION {
    x4n::log::info("X4MCP extension loaded (v0.1.0)");

    // Load config from x4mcp_config.json next to the DLL
    g_extension_path = x4n::path();
    g_config = x4mcp::Config::load(g_extension_path.c_str());

    // Restore game_started flag from stash (survives /reloadui).
    // Stash remembers the event fired, but we may have returned to the main
    // menu since then. Validate with GetPlayerID before trusting it.
    if (restore_game_started()) {
        auto* game = x4n::game();
        if (game && game->GetPlayerID && game->GetPlayerID() != 0) {
            g_commands.set_game_started(true);
        } else {
            // Stash was stale — player left the game
            persist_game_started(false);
        }
    }

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
    x4n::on("on_universe_ready", on_game_started);
    x4n::on("on_frame_update", on_frame_update);
}

X4N_SHUTDOWN {
    x4n::log::info("X4MCP shutting down");
    if (g_server) {
        g_server->stop();
        g_server.reset();
    }
}
