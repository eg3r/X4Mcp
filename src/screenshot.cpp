// ---------------------------------------------------------------------------
// screenshot.cpp — native (engine) screenshot.
//
// DEV / LLM-TEST TOOLING ONLY. Arms X4's own single-shot screenshot via the
// X4Native internal-function resolver, then locates the file the render thread
// writes. The arm (Screenshot_ArmSingleShot) MUST be called on the game UI
// thread; see game_commands.cpp. RE notes: docs report x4_native_screenshot_RE.
// ---------------------------------------------------------------------------
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>     // SHGetKnownFolderPath, FOLDERID_Documents
#include <objbase.h>    // CoInitializeEx / CoCreateInstance
#include <wincodec.h>   // WIC — PNG alpha-flatten
#include <wrl/client.h> // ComPtr

#include <cstdio>
#include <cwctype>
#include <string>

#include <x4native.h> // x4n::game_internal, x4n::log

#include "screenshot.h"

namespace x4mcp {
namespace {

// Engine arm function: void __fastcall Screenshot_ArmSingleShot(std::string* subdir).
using ArmFn = void(__fastcall*)(const void*);
ArmFn g_arm = nullptr;
bool  g_arm_resolved = false;

// MSVC Release-ABI std::string (32 bytes). A zero-initialised value with
// size==0 is a valid empty (SSO) string — which arms a plain top-level shot.
struct GameString {
    char     buf[16];
    uint64_t size;
    uint64_t cap;
};

// ~3s of FILETIME ticks (100ns units) — tolerance for clock/granularity skew
// between the arm timestamp and the file's recorded write time.
constexpr uint64_t kTimeTolerance = 30'000'000ULL;

ArmFn resolve_arm() {
    if (!g_arm_resolved) {
        g_arm = reinterpret_cast<ArmFn>(x4n::game_internal("Screenshot_ArmSingleShot"));
        g_arm_resolved = true;
        if (!g_arm) {
            x4n::log::info("capture_screenshot: Screenshot_ArmSingleShot not resolved "
                           "(missing RVA for this game build?)");
        }
    }
    return g_arm;
}

std::wstring widen(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), n);
    return w;
}

std::string narrow(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                                nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

bool is_screenshot_ext(const std::wstring& name) {
    auto dot = name.find_last_of(L'.');
    if (dot == std::wstring::npos) return false;
    std::wstring ext = name.substr(dot + 1);
    for (auto& c : ext) c = (wchar_t)towlower(c);
    return ext == L"png" || ext == L"jpg" || ext == L"jpeg" || ext == L"bmp";
}

// Resolve <Documents>\Egosoft\X4\<profile>\screenshots, choosing the most
// recently used numeric profile (the active save profile).
std::wstring resolve_screenshots_dir() {
    PWSTR docs = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &docs))) {
        return {};
    }
    std::wstring base = std::wstring(docs) + L"\\Egosoft\\X4";
    CoTaskMemFree(docs);

    std::wstring best_profile;
    FILETIME best_time{};
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW((base + L"\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            if (fd.cFileName[0] == L'.') continue;
            if (CompareFileTime(&fd.ftLastWriteTime, &best_time) > 0) {
                best_time = fd.ftLastWriteTime;
                best_profile = fd.cFileName;
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    if (best_profile.empty()) return {};
    return base + L"\\" + best_profile + L"\\screenshots";
}

// Find this process's main game window (largest visible, unowned, titled
// top-level) — for bringing X4 forward so it renders before a capture.
struct EnumCtx { DWORD pid; HWND best; long long area; };

BOOL CALLBACK enum_proc(HWND hwnd, LPARAM lp) {
    auto* ctx = reinterpret_cast<EnumCtx*>(lp);
    DWORD wpid = 0;
    GetWindowThreadProcessId(hwnd, &wpid);
    if (wpid != ctx->pid) return TRUE;
    if (!IsWindowVisible(hwnd)) return TRUE;
    if (GetWindow(hwnd, GW_OWNER) != nullptr) return TRUE;
    if (GetWindowTextLengthW(hwnd) == 0) return TRUE;
    RECT r{};
    if (!GetWindowRect(hwnd, &r)) return TRUE;
    long long a = (long long)(r.right - r.left) * (r.bottom - r.top);
    if (a > ctx->area) { ctx->area = a; ctx->best = hwnd; }
    return TRUE;
}

HWND find_main_window() {
    EnumCtx ctx{ GetCurrentProcessId(), nullptr, 0 };
    EnumWindows(enum_proc, reinterpret_cast<LPARAM>(&ctx));
    return ctx.best;
}

} // namespace

std::string arm_native_screenshot() {
    ArmFn arm = resolve_arm();
    if (!arm) return "error:Screenshot_ArmSingleShot not resolved for this build";

    GameString empty{};   // buf zeroed
    empty.size = 0;       // empty string -> plain screenshot (no sub-dir)
    empty.cap = 15;       // SSO capacity

    // FFI-pre-verify: bracket the internal call so a crash is localizable.
    x4n::log::info("capture_screenshot: arming native screenshot");
    arm(&empty);
    x4n::log::info("capture_screenshot: armed");
    return "ok";
}

void foreground_game_window() {
    // X4 does not render the 3D scene while backgrounded/focus-paused, so a
    // capture taken then is blank. Bring the game window to the foreground.
    // AttachThreadInput unlocks SetForegroundWindow from a background thread.
    HWND hwnd = find_main_window();
    if (!hwnd) return;
    HWND fg = GetForegroundWindow();
    DWORD fg_thread = fg ? GetWindowThreadProcessId(fg, nullptr) : 0;
    DWORD my_thread = GetCurrentThreadId();
    bool attached = fg_thread && fg_thread != my_thread
                  && AttachThreadInput(my_thread, fg_thread, TRUE);
    if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);
    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);
    if (attached) AttachThreadInput(my_thread, fg_thread, FALSE);
}

uint64_t now_filetime() {
    FILETIME ft{};
    GetSystemTimeAsFileTime(&ft);
    return ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
}

bool find_screenshot_after(uint64_t since, std::string& out_path, std::string& err) {
    std::wstring dir = resolve_screenshots_dir();
    if (dir.empty()) { err = "could not resolve the X4 screenshots directory"; return false; }

    uint64_t floor = (since > kTimeTolerance) ? since - kTimeTolerance : 0;

    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW((dir + L"\\screen_*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        err = "no screenshots in " + narrow(dir);
        return false;
    }

    std::wstring best;
    uint64_t best_time = 0;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (!is_screenshot_ext(fd.cFileName)) continue;
        if (fd.nFileSizeLow == 0 && fd.nFileSizeHigh == 0) continue;  // skip 0-byte placeholders
        uint64_t t = ((uint64_t)fd.ftLastWriteTime.dwHighDateTime << 32)
                   | fd.ftLastWriteTime.dwLowDateTime;
        if (t >= floor && t >= best_time) {
            best_time = t;
            best = fd.cFileName;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);

    if (best.empty()) { err = "no new screenshot file yet"; return false; }
    out_path = narrow(dir + L"\\" + best);
    return true;
}

bool flatten_png_alpha(const std::string& path, std::string& err) {
    using Microsoft::WRL::ComPtr;

    // COM init (MTA) for this thread; tolerate already-initialized.
    HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool need_uninit = SUCCEEDED(co);  // S_OK/S_FALSE → balance with CoUninitialize

    std::wstring wpath = widen(path);
    HRESULT hr = S_OK;
    {
        ComPtr<IWICImagingFactory> factory;
        hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                              CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));

        // Decode the captured PNG and convert 32bpp(+alpha) → 24bppBGR (drops
        // alpha, keeps the full RGB), caching pixels in memory so we can
        // overwrite the same file.
        ComPtr<IWICBitmap> cached;
        UINT w = 0, h = 0;
        if (SUCCEEDED(hr)) {
            ComPtr<IWICBitmapDecoder> decoder;
            hr = factory->CreateDecoderFromFilename(wpath.c_str(), nullptr, GENERIC_READ,
                                                    WICDecodeMetadataCacheOnLoad, &decoder);
            ComPtr<IWICBitmapFrameDecode> frame;
            if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, &frame);
            ComPtr<IWICFormatConverter> conv;
            if (SUCCEEDED(hr)) hr = factory->CreateFormatConverter(&conv);
            if (SUCCEEDED(hr)) hr = conv->Initialize(frame.Get(), GUID_WICPixelFormat24bppBGR,
                                                     WICBitmapDitherTypeNone, nullptr, 0.0,
                                                     WICBitmapPaletteTypeMedianCut);
            if (SUCCEEDED(hr)) hr = factory->CreateBitmapFromSource(conv.Get(),
                                                                    WICBitmapCacheOnLoad, &cached);
            if (SUCCEEDED(hr)) hr = cached->GetSize(&w, &h);
            // decoder/frame/conv released here → file handle closed before we rewrite it.
        }

        // Re-encode the opaque bitmap back to the same path.
        if (SUCCEEDED(hr)) {
            ComPtr<IWICStream> stream;
            hr = factory->CreateStream(&stream);
            if (SUCCEEDED(hr)) hr = stream->InitializeFromFilename(wpath.c_str(), GENERIC_WRITE);
            ComPtr<IWICBitmapEncoder> encoder;
            if (SUCCEEDED(hr)) hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
            if (SUCCEEDED(hr)) hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
            ComPtr<IWICBitmapFrameEncode> fe;
            ComPtr<IPropertyBag2> props;
            if (SUCCEEDED(hr)) hr = encoder->CreateNewFrame(&fe, &props);
            if (SUCCEEDED(hr)) hr = fe->Initialize(props.Get());
            if (SUCCEEDED(hr)) hr = fe->SetSize(w, h);
            WICPixelFormatGUID fmt = GUID_WICPixelFormat24bppBGR;
            if (SUCCEEDED(hr)) hr = fe->SetPixelFormat(&fmt);
            if (SUCCEEDED(hr)) hr = fe->WriteSource(cached.Get(), nullptr);
            if (SUCCEEDED(hr)) hr = fe->Commit();
            if (SUCCEEDED(hr)) hr = encoder->Commit();
        }
    }

    if (need_uninit) CoUninitialize();

    if (FAILED(hr)) {
        char buf[80];
        _snprintf_s(buf, sizeof(buf), _TRUNCATE, "WIC alpha-flatten failed (hr=0x%08lX)",
                    (unsigned long)hr);
        err = buf;
        return false;
    }
    return true;
}

} // namespace x4mcp
