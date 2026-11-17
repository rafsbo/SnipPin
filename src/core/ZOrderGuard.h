// Keeps pinned windows above everything else.
//
// Uses out-of-context WinEvent hooks (no DLL injection): when the foreground
// window changes, a fullscreen app may have pushed itself to the top of the
// topmost band, so pinned windows are re-inserted at HWND_TOPMOST after a
// short debounce. A slow periodic check catches anything the events missed.
// Re-assertion is rate limited so two "always on top" tools cannot end up
// fighting in a tight loop.
#pragma once

#include <cstdint>
#include <functional>

#include "platform/Win32.h"

namespace snippin {

class ZOrderGuard {
public:
    using WindowEventHandler = std::function<void(DWORD event, HWND hwnd)>;
    using ReassertHandler = std::function<void(bool force)>;

    ZOrderGuard() = default;
    ~ZOrderGuard();
    ZOrderGuard(const ZOrderGuard&) = delete;
    ZOrderGuard& operator=(const ZOrderGuard&) = delete;

    // |host| receives WM_TIMER messages that must be forwarded to OnTimer().
    bool Start(HWND host, WindowEventHandler onWindowEvent, ReassertHandler onReassert);
    void Stop();

    void SetEnabled(bool enabled, int intervalMs);
    bool OnTimer(UINT_PTR timerId);  // true when the timer belonged to the guard

    // Requests a debounced re-assertion (e.g. after the foreground changed).
    void RequestReassert();

    static constexpr UINT_PTR kDebounceTimer = 0x5A01;
    static constexpr UINT_PTR kPeriodicTimer = 0x5A02;

private:
    static void CALLBACK WinEventProc(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG idObject,
                                      LONG idChild, DWORD thread, DWORD time);
    void HandleEvent(DWORD event, HWND hwnd);
    bool AllowReassert();

    HWND host_ = nullptr;
    HWINEVENTHOOK foregroundHook_ = nullptr;
    HWINEVENTHOOK minimizeHook_ = nullptr;
    HWINEVENTHOOK objectHook_ = nullptr;
    WindowEventHandler onWindowEvent_;
    ReassertHandler onReassert_;
    bool enabled_ = true;
    int intervalMs_ = 1000;

    // Rate limiting: at most kMaxPerWindow re-assertions per kWindowMs.
    static constexpr int kMaxPerWindow = 15;
    static constexpr std::uint64_t kWindowMs = 1000;
    std::uint64_t rateWindowStart_ = 0;
    int rateCount_ = 0;

    static ZOrderGuard* instance_;
};

}  // namespace snippin
