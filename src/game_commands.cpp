// ---------------------------------------------------------------------------
// game_commands.cpp — Thread-safe command queue for game operations
// ---------------------------------------------------------------------------
#include "game_commands.h"

#include <chrono>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace x4mcp {

std::string GameCommandQueue::execute(CommandType type, const std::string& param,
                                       int timeout_ms) {
    auto promise = std::make_shared<std::promise<std::string>>();
    auto future = promise->get_future();

    GameCommand cmd;
    cmd.type = type;
    cmd.param = param;
    cmd.result = promise;

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        queue_.push(std::move(cmd));
    }

    auto status = future.wait_for(std::chrono::milliseconds(timeout_ms));
    if (status == std::future_status::timeout) {
        return "error:timeout waiting for game thread";
    }
    return future.get();
}

void GameCommandQueue::process_pending() {
    std::queue<GameCommand> local_queue;
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        std::swap(local_queue, queue_);
    }

    while (!local_queue.empty()) {
        auto cmd = std::move(local_queue.front());
        local_queue.pop();

        switch (cmd.type) {
        case CommandType::IsGameLoaded: {
            auto* game = x4n::game();
            bool loaded = false;
            if (game && game->GetPlayerID) {
                auto pid = game->GetPlayerID();
                loaded = (pid != 0);
            }
            cmd.result->set_value(loaded ? "true" : "false");
            break;
        }

        case CommandType::LoadGame: {
            {
                std::lock_guard<std::mutex> lock(lua_mutex_);
                pending_lua_["x4mcp.load_game_result"] = cmd.result;
            }
            x4n::raise_lua("x4mcp.load_game", cmd.param.c_str());
            break;
        }

        case CommandType::ShutdownGame: {
            {
                std::lock_guard<std::mutex> lock(lua_mutex_);
                pending_lua_["x4mcp.quit_game_result"] = cmd.result;
            }
            x4n::raise_lua("x4mcp.quit_game");
            break;
        }

        case CommandType::ExecuteChatCommand: {
            {
                std::lock_guard<std::mutex> lock(lua_mutex_);
                pending_lua_["x4mcp.chat_command_result"] = cmd.result;
            }
            x4n::raise_lua("x4mcp.chat_command", cmd.param.c_str());
            break;
        }

        case CommandType::ListSaves: {
            {
                std::lock_guard<std::mutex> lock(lua_mutex_);
                pending_lua_["x4mcp.list_saves_result"] = cmd.result;
            }
            x4n::raise_lua("x4mcp.list_saves");
            break;
        }

        case CommandType::StashGet: {
            const char* val = x4n::stash::get_string(cmd.param.c_str());
            cmd.result->set_value(val ? val : "");
            break;
        }

        case CommandType::StashSet: {
            // param is JSON: {"key":"...","value":"..."}
            try {
                auto j = json::parse(cmd.param);
                auto key = j.value("key", "");
                auto value = j.value("value", "");
                bool ok = x4n::stash::set_string(key.c_str(), value.c_str());
                cmd.result->set_value(ok ? "ok" : "error:stash_set failed");
            } catch (...) {
                cmd.result->set_value("error:invalid JSON param");
            }
            break;
        }

        case CommandType::StashDelete: {
            bool found = x4n::stash::remove(cmd.param.c_str());
            cmd.result->set_value(found ? "ok" : "not_found");
            break;
        }

        case CommandType::Log: {
            try {
                auto j = json::parse(cmd.param);
                auto message = j.value("message", "");
                auto filename = j.value("filename", "");
                auto level = j.value("level", "info");

                int log_level = X4NATIVE_LOG_INFO;
                if (level == "debug") log_level = X4NATIVE_LOG_DEBUG;
                else if (level == "warn")  log_level = X4NATIVE_LOG_WARN;
                else if (level == "error") log_level = X4NATIVE_LOG_ERROR;

                if (filename.empty()) {
                    x4n::log::detail::write(log_level, message.c_str());
                } else {
                    x4n::log::detail::write_named(log_level, message.c_str(), filename.c_str());
                }
                cmd.result->set_value("ok");
            } catch (...) {
                cmd.result->set_value("error:invalid JSON param");
            }
            break;
        }
        }
    }
}

void GameCommandQueue::on_lua_result(const std::string& event_name,
                                      const std::string& result) {
    std::lock_guard<std::mutex> lock(lua_mutex_);
    auto it = pending_lua_.find(event_name);
    if (it != pending_lua_.end()) {
        it->second->set_value(result);
        pending_lua_.erase(it);
    }
}

} // namespace x4mcp
