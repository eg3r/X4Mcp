# X4Mcp

![C++23](https://img.shields.io/badge/C%2B%2B-23-blue?style=flat-square&logo=cplusplus)
![Platform](https://img.shields.io/badge/platform-Windows%20x64-lightgrey?style=flat-square&logo=windows)
![License](https://img.shields.io/badge/license-MIT-green?style=flat-square)
![CI](https://github.com/eg3r/X4MCP/actions/workflows/ci.yml/badge.svg)
![Release](https://github.com/eg3r/X4MCP/actions/workflows/release.yml/badge.svg)

A [Model Context Protocol](https://modelcontextprotocol.io/) (MCP) server for **X4: Foundations** — lets AI assistants and external tools control a running game instance over HTTP.

X4MCP runs inside the game process as an [X4Native](https://github.com/eg3r/X4Native) extension, exposing game actions through a standard MCP interface. Connect your LLM, or any MCP-compatible client, and interact with your game: load saves, run commands, query game state, and more.

---

## Prerequisites

- **[X4Native](https://github.com/eg3r/X4Native)** — must be installed first (X4Mcp is an X4Native extension)
- X4: Foundation
- Protected UI mode **OFF** in game settings

---

## Installation

1. Download the latest release from [Releases](https://github.com/eg3r/X4Mcp/releases)
2. Extract into your X4 `extensions/` folder (e.g. `X4 Foundations/extensions/x4mcp/`)
3. Launch the game — the MCP server starts automatically on `127.0.0.1:7779`

### Configuration

Edit `x4mcp_config.json` in the extension folder to change the server bind address or port:

```json
{
  "port": 7779,
  "bind_address": "127.0.0.1"
}
```

---

## Available Tools

Once connected, MCP clients can call the following tools:

| Tool | Description |
|------|-------------|
| `is_game_loaded` | Check whether a savegame is currently loaded |
| `load_game` | Load a savegame by filename, display name, or `"latest"` |
| `shutdown_game` | Quit the X4 application |
| `execute_chat_command` | Run a debug/chat command (e.g. `thereshallbelight`) |
| `list_saves` | List available savegames (up to 20 most recent) |
| `stash_get` | Retrieve a persistent key-value pair (survives `/reloadui`) (X4Native stash)|
| `stash_set` | Store a persistent key-value pair (X4Native stash)|
| `stash_delete` | Delete a persistent key-value pair (X4Native stash)|
| `log` | Write a log message with configurable level and filename |

---

## Building from Source

### Requirements

- Any C++ compiler-toolchain that is supported by CMake
- CMake 3.20+

### Build & Deploy

```powershell
.\scripts\build_deploy.ps1            # Debug build + deploy to game
.\scripts\build_deploy.ps1 -Release   # Release build + deploy
.\scripts\build_deploy.ps1 -Clean     # Clean rebuild
```

Or manually:

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
cmake --build build --config Release --target deploy
```

The deploy target auto-detects your X4 installation from the Steam registry. Pass `-DX4_GAME_DIR=<path>` if auto-detection fails.

---

## Contributing

Contributions are welcome! Fork the repo, create a feature branch, and open a pull request.

1. Fork & clone the repository
2. Create a branch (`git checkout -b my-feature`)
3. Build and test locally with `.\scripts\build_deploy.ps1`
4. Commit your changes and open a PR against `main`

---

## License

MIT — see [LICENSE](LICENSE).
