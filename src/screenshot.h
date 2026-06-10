#pragma once
// ---------------------------------------------------------------------------
// screenshot.h — native (engine) screenshot for the MCP capture_screenshot tool.
//
// DEV / LLM-TEST TOOLING ONLY. Triggers X4's OWN screenshot via the X4Native
// internal-function resolver (Screenshot_ArmSingleShot, version_db). The engine
// writes screenshots/screen_<datetime>.<ext> (PNG by default) including the UI
// and works in exclusive fullscreen.
//
// The ARM must run on the game UI thread (GameCommandQueue::process_pending).
// File discovery runs on the HTTP thread after the (async) render-thread write.
// ---------------------------------------------------------------------------
#include <string>
#include <cstdint>

namespace x4mcp {

// Arm a single native screenshot. MUST be called on the game UI thread.
// Resolves Screenshot_ArmSingleShot via the X4Native version_db on first use.
// Returns "ok", or "error:<reason>".
std::string arm_native_screenshot();

// Bring the X4 window to the foreground so the engine renders the 3D scene
// (it does not while backgrounded/focus-paused → blank capture). Call before
// arming. Safe to call from any thread.
void foreground_game_window();

// Current time as a Windows FILETIME (100ns ticks since 1601), captured before
// arming so file discovery can ignore pre-existing screenshots.
uint64_t now_filetime();

// Find the newest screen_*.{png,jpg,jpeg,bmp} under the X4 user screenshots
// directory whose last-write time is at/after `since` (minus a small tolerance).
// On success sets out_path to the absolute path and returns true.
bool find_screenshot_after(uint64_t since, std::string& out_path, std::string& err);

// Re-encode a PNG dropping its alpha channel (24-bit BGR), in place. X4's native
// capture bakes a circular cockpit/UI vignette into alpha; flattening to opaque
// yields the clean image the game's own screenshot path produces. Returns true
// on success; on failure the original (alpha-masked) file is left untouched.
bool flatten_png_alpha(const std::string& path, std::string& err);

} // namespace x4mcp
