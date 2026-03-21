-- x4mcp.lua — Lua bridge for X4MCP extension
-- Loaded via extension/ui.xml in the menus environment.
--
-- Bridges between C++ (MCP server) and Lua-only game APIs:
--   - LoadGame(filename)   — global Lua function, not in FFI
--   - GetSaveList(filter)  — global Lua function, returns save table
--   - QuitGame()           — global Lua function
--   - ExecuteDebugCommand(verb, params) — slash command execution
--
-- C++ raises Lua events; Lua executes game functions and reports back.

local ffi = require("ffi")
local C = ffi.C
pcall(ffi.cdef, [[
    typedef uint64_t UniverseID;
    bool IsPlayerValid(void);
    UniverseID GetPlayerID(void);
    bool IsSaveListLoadingComplete(void);
    void ReloadSaveList(void);
]])

-- ---------------------------------------------------------------------------
-- Load game bridge
-- C++ fires "x4mcp.load_game" with a save name or "latest".
-- Lua finds the save, calls LoadGame(filename), reports result.
-- ---------------------------------------------------------------------------
local _x4mcp_pending_load = nil

RegisterEvent("x4mcp.load_game", function(_, save_name)
    if not save_name or save_name == "" then
        CallEventScripts("x4mcp.load_game_result", "error:no save name provided")
        return
    end
    _x4mcp_pending_load = save_name
end)

local function x4mcp_try_load_game()
    if not _x4mcp_pending_load then return end

    -- Wait for save list to be ready
    local ok, complete = pcall(C.IsSaveListLoadingComplete)
    if not ok or not complete then return end

    local save_name = _x4mcp_pending_load
    _x4mcp_pending_load = nil

    local savegames = GetSaveList()
    if not savegames or #savegames == 0 then
        CallEventScripts("x4mcp.load_game_result", "error:no saves found")
        return
    end

    -- Sort by time (newest first) for "latest" lookup
    table.sort(savegames, function(a, b) return a.rawtime > b.rawtime end)

    local target = nil
    if save_name == "latest" then
        target = savegames[1]
    else
        -- Search by filename first, then by displayed name / location
        local name_lower = string.lower(save_name)
        for _, save in ipairs(savegames) do
            if string.lower(save.filename) == name_lower then
                target = save
                break
            end
        end
        if not target then
            for _, save in ipairs(savegames) do
                local loc = save.location or ""
                local desc = save.description or ""
                if string.find(string.lower(loc), name_lower, 1, true)
                   or string.find(string.lower(desc), name_lower, 1, true)
                   or string.find(string.lower(save.filename), name_lower, 1, true) then
                    target = save
                    break
                end
            end
        end
    end

    if not target then
        CallEventScripts("x4mcp.load_game_result", "error:save not found: " .. save_name)
        return
    end

    CallEventScripts("x4mcp.load_game_result", "ok:" .. target.filename)
    LoadGame(target.filename)
end

-- ---------------------------------------------------------------------------
-- Quit game bridge
-- C++ fires "x4mcp.quit_game"
-- ---------------------------------------------------------------------------
RegisterEvent("x4mcp.quit_game", function()
    CallEventScripts("x4mcp.quit_game_result", "ok")
    QuitGame()
end)

-- ---------------------------------------------------------------------------
-- Chat command bridge
-- C++ fires "x4mcp.chat_command" with a command string like "connect 192.168.1.1"
-- Parses into verb + params and calls ExecuteDebugCommand.
-- ---------------------------------------------------------------------------
RegisterEvent("x4mcp.chat_command", function(_, command_str)
    if not command_str or command_str == "" then
        CallEventScripts("x4mcp.chat_command_result", "error:empty command")
        return
    end

    -- Strip leading "/" if present
    if string.sub(command_str, 1, 1) == "/" then
        command_str = string.sub(command_str, 2)
    end

    local space = string.find(command_str, " ")
    local verb, params
    if space then
        verb = string.sub(command_str, 1, space - 1)
        params = string.sub(command_str, space + 1)
    else
        verb = command_str
        params = ""
    end

    ExecuteDebugCommand(verb, params)
    CallEventScripts("x4mcp.chat_command_result", "ok:" .. verb)
end)

-- ---------------------------------------------------------------------------
-- Save list bridge
-- C++ fires "x4mcp.list_saves" to get available save games.
-- Returns pipe-delimited list: "filename1|location1|time1||filename2|location2|time2||..."
-- ---------------------------------------------------------------------------
RegisterEvent("x4mcp.list_saves", function()
    local ok, complete = pcall(C.IsSaveListLoadingComplete)
    if not ok or not complete then
        C.ReloadSaveList()
        CallEventScripts("x4mcp.list_saves_result", "error:save list loading")
        return
    end

    local savegames = GetSaveList()
    if not savegames or #savegames == 0 then
        CallEventScripts("x4mcp.list_saves_result", "empty")
        return
    end

    table.sort(savegames, function(a, b) return a.rawtime > b.rawtime end)

    local parts = {}
    for i, save in ipairs(savegames) do
        if i > 20 then break end  -- limit to 20 most recent
        table.insert(parts, save.filename .. "|" .. (save.location or "") .. "|" .. tostring(save.rawtime))
    end
    CallEventScripts("x4mcp.list_saves_result", "ok:" .. table.concat(parts, "||"))
end)

-- ---------------------------------------------------------------------------
-- Per-frame update — process pending load requests
-- ---------------------------------------------------------------------------
local function x4mcp_on_update()
    x4mcp_try_load_game()
end

SetScript("onUpdate", x4mcp_on_update)
