// ---------------------------------------------------------------------------
// mcp_server.cpp — MCP server implementation
// ---------------------------------------------------------------------------
#include "mcp_server.h"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <x4native.h>

using json = nlohmann::json;

namespace x4mcp {

// ---------------------------------------------------------------------------
// Tool definitions
// ---------------------------------------------------------------------------
static json make_tools_list() {
    return json::array({
        {
            {"name", "is_game_loaded"},
            {"description", "Check if an X4 game/savegame is currently loaded and the player is active."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", json::object()},
            }},
        },
        {
            {"name", "load_game"},
            {"description", "Load a savegame by filename, display name, or 'latest' for the most recent save."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"name", {
                        {"type", "string"},
                        {"description", "Save name, filename, or 'latest'"},
                    }},
                }},
                {"required", json::array({"name"})},
            }},
        },
        {
            {"name", "shutdown_game"},
            {"description", "Quit the X4 game application."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", json::object()},
            }},
        },
        {
            {"name", "execute_chat_command"},
            {"description", "Execute a chat/debug command in the game. Examples: 'connect 192.168.1.1', 'host', 'thereshallbelight'."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"command", {
                        {"type", "string"},
                        {"description", "The command to execute (without leading /)"},
                    }},
                }},
                {"required", json::array({"command"})},
            }},
        },
        {
            {"name", "list_saves"},
            {"description", "List available savegames (up to 20 most recent)."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", json::object()},
            }},
        },
        {
            {"name", "stash_get"},
            {"description", "Retrieve a value from persistent stash (survives /reloadui and hot-reload, lost on game exit)."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"key", {
                        {"type", "string"},
                        {"description", "The stash key to retrieve"},
                    }},
                }},
                {"required", json::array({"key"})},
            }},
        },
        {
            {"name", "stash_set"},
            {"description", "Store a value in persistent stash (survives /reloadui and hot-reload, lost on game exit)."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"key", {
                        {"type", "string"},
                        {"description", "The stash key to store under"},
                    }},
                    {"value", {
                        {"type", "string"},
                        {"description", "The string value to store"},
                    }},
                }},
                {"required", json::array({"key", "value"})},
            }},
        },
        {
            {"name", "stash_delete"},
            {"description", "Delete a key from persistent stash."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"key", {
                        {"type", "string"},
                        {"description", "The stash key to delete"},
                    }},
                }},
                {"required", json::array({"key"})},
            }},
        },
        {
            {"name", "log"},
            {"description", "Write a log message to a file in the extension folder. Uses x4native's named-file logging."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"message", {
                        {"type", "string"},
                        {"description", "The log message to write"},
                    }},
                    {"filename", {
                        {"type", "string"},
                        {"description", "Target log filename in the extension folder (e.g. 'debug.log'). If omitted, writes to the extension's default log."},
                    }},
                    {"level", {
                        {"type", "string"},
                        {"enum", json::array({"debug", "info", "warn", "error"})},
                        {"description", "Log level (default: 'info')"},
                    }},
                }},
                {"required", json::array({"message"})},
            }},
        },
    });
}

// ---------------------------------------------------------------------------
// JSON-RPC helpers
// ---------------------------------------------------------------------------
static json make_response(const json& id, const json& result) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"result", result}};
}

static json make_error(const json& id, int code, const std::string& message) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"error", {{"code", code}, {"message", message}}},
    };
}

static json make_tool_result(const std::string& text, bool is_error = false) {
    return {
        {"content", json::array({{{"type", "text"}, {"text", text}}})},
        {"isError", is_error},
    };
}

// ---------------------------------------------------------------------------
// Request handling
// ---------------------------------------------------------------------------
static json handle_initialize(const json& id) {
    return make_response(id, {
        {"protocolVersion", "2025-03-26"},
        {"capabilities", {
            {"tools", {{"listChanged", false}}},
        }},
        {"serverInfo", {
            {"name", "x4mcp"},
            {"version", "0.1.0"},
        }},
    });
}

static json handle_tools_list(const json& id) {
    return make_response(id, {{"tools", make_tools_list()}});
}

static json handle_tools_call(const json& id, const json& params,
                               GameCommandQueue& commands) {
    auto tool_name = params.value("name", "");
    auto arguments = params.value("arguments", json::object());

    if (tool_name == "is_game_loaded") {
        auto result = commands.execute(CommandType::IsGameLoaded);
        return make_response(id, make_tool_result(result));
    }

    if (tool_name == "load_game") {
        auto name = arguments.value("name", "");
        if (name.empty()) {
            return make_response(id, make_tool_result(
                "error: 'name' parameter is required", true));
        }
        auto result = commands.execute(CommandType::LoadGame, name);
        return make_response(id, make_tool_result(result));
    }

    if (tool_name == "shutdown_game") {
        auto result = commands.execute(CommandType::ShutdownGame);
        return make_response(id, make_tool_result(result));
    }

    if (tool_name == "execute_chat_command") {
        auto command = arguments.value("command", "");
        if (command.empty()) {
            return make_response(id, make_tool_result(
                "error: 'command' parameter is required", true));
        }
        auto result = commands.execute(CommandType::ExecuteChatCommand, command);
        return make_response(id, make_tool_result(result));
    }

    if (tool_name == "list_saves") {
        auto result = commands.execute(CommandType::ListSaves);
        return make_response(id, make_tool_result(result));
    }

    if (tool_name == "stash_get") {
        auto key = arguments.value("key", "");
        if (key.empty()) {
            return make_response(id, make_tool_result(
                "error: 'key' parameter is required", true));
        }
        auto result = commands.execute(CommandType::StashGet, key);
        if (result.empty()) {
            return make_response(id, make_tool_result("not_found"));
        }
        return make_response(id, make_tool_result(result));
    }

    if (tool_name == "stash_set") {
        auto key = arguments.value("key", "");
        auto value = arguments.value("value", "");
        if (key.empty()) {
            return make_response(id, make_tool_result(
                "error: 'key' parameter is required", true));
        }
        // Pack key+value as JSON for the command queue
        json param = {{"key", key}, {"value", value}};
        auto result = commands.execute(CommandType::StashSet, param.dump());
        return make_response(id, make_tool_result(result));
    }

    if (tool_name == "stash_delete") {
        auto key = arguments.value("key", "");
        if (key.empty()) {
            return make_response(id, make_tool_result(
                "error: 'key' parameter is required", true));
        }
        auto result = commands.execute(CommandType::StashDelete, key);
        return make_response(id, make_tool_result(result));
    }

    if (tool_name == "log") {
        auto message = arguments.value("message", "");
        if (message.empty()) {
            return make_response(id, make_tool_result(
                "error: 'message' parameter is required", true));
        }
        json param = {
            {"message", message},
            {"filename", arguments.value("filename", "")},
            {"level", arguments.value("level", "info")},
        };
        auto result = commands.execute(CommandType::Log, param.dump());
        return make_response(id, make_tool_result(result));
    }

    return make_error(id, -32602, "Unknown tool: " + tool_name);
}

static json handle_request(const json& request, GameCommandQueue& commands) {
    auto method = request.value("method", "");
    auto id = request.value("id", json(nullptr));
    auto params = request.value("params", json::object());

    if (method == "initialize") {
        return handle_initialize(id);
    }
    if (method == "notifications/initialized") {
        // Client notification — no response needed
        return json();
    }
    if (method == "tools/list") {
        return handle_tools_list(id);
    }
    if (method == "tools/call") {
        return handle_tools_call(id, params, commands);
    }

    return make_error(id, -32601, "Method not found: " + method);
}

// ---------------------------------------------------------------------------
// Server implementation
// ---------------------------------------------------------------------------
struct McpServer::Impl {
    httplib::Server server;
};

McpServer::McpServer(GameCommandQueue& commands, uint16_t port,
                     const std::string& bind_address)
    : commands_(commands), port_(port), bind_address_(bind_address),
      impl_(std::make_unique<Impl>()) {}

McpServer::~McpServer() {
    stop();
}

bool McpServer::start() {
    if (running_) return true;

    // MCP endpoint
    impl_->server.Post("/mcp", [this](const httplib::Request& req,
                                       httplib::Response& res) {
        try {
            auto request = json::parse(req.body);
            auto response = handle_request(request, commands_);
            if (response.is_null()) {
                // Notification — no response body
                res.status = 204;
                return;
            }
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            auto err = make_error(nullptr, -32700,
                                   std::string("Parse error: ") + e.what());
            res.set_content(err.dump(), "application/json");
            res.status = 400;
        }
    });

    // Health check
    impl_->server.Get("/health", [](const httplib::Request&,
                                     httplib::Response& res) {
        res.set_content(R"({"status":"ok"})", "application/json");
    });

    running_ = true;
    server_thread_ = std::thread([this]() {
        x4n::log::info("MCP server starting on %s:%d", bind_address_.c_str(), port_);
        if (!impl_->server.listen(bind_address_, port_)) {
            x4n::log::error("MCP server failed to bind to port %d", port_);
            running_ = false;
        }
    });

    return true;
}

void McpServer::stop() {
    if (!running_) return;
    running_ = false;
    impl_->server.stop();
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    x4n::log::info("MCP server stopped");
}

} // namespace x4mcp
