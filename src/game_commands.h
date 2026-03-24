#pragma once
// ---------------------------------------------------------------------------
// game_commands.h — Thread-safe command queue for game operations
//
// The MCP HTTP server runs on a background thread. Game API calls must happen
// on the UI thread. This module bridges the two:
//   1. HTTP thread enqueues a GameCommand
//   2. UI thread (on_frame_update) dequeues and executes
//   3. For synchronous commands (IsGameLoaded): result set immediately
//   4. For Lua-async commands (LoadGame, etc.): result set when Lua responds
//      via event callback in a later frame
// ---------------------------------------------------------------------------
#include <x4native.h>

#include <string>
#include <future>
#include <mutex>
#include <queue>
#include <unordered_map>

namespace x4mcp {

enum class CommandType {
    IsGameLoaded,
    LoadGame,
    ShutdownGame,
    ExecuteChatCommand,
    ListSaves,
    StashGet,
    StashSet,
    StashDelete,
    Log,
};

struct GameCommand {
    CommandType type;
    std::string param;
    std::shared_ptr<std::promise<std::string>> result;
};

class GameCommandQueue {
public:
    // Called from HTTP thread — enqueues command and waits for result.
    // Returns the result string. Blocks up to timeout_ms.
    std::string execute(CommandType type, const std::string& param = "",
                        int timeout_ms = 15000);

    // Called from UI thread (on_frame_update) — processes pending commands.
    void process_pending();

    // Called from UI thread when Lua reports a result via event bridge.
    void on_lua_result(const std::string& event_name, const std::string& result);

    // Set by main.cpp when on_game_started fires (world fully ready).
    void set_game_started(bool started) { game_started_ = started; }

private:
    bool game_started_ = false;
    std::mutex queue_mutex_;
    std::queue<GameCommand> queue_;

    // Outstanding Lua async results: event_name -> promise
    std::mutex lua_mutex_;
    std::unordered_map<std::string, std::shared_ptr<std::promise<std::string>>>
        pending_lua_;
};

} // namespace x4mcp
