// Owns every modification SnipPin applies to windows of other applications:
// always-on-top, opacity and ghost (click-through) mode. Original state is
// captured on first touch and restored when the window is released.
#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "core/RecoveryJournal.h"
#include "logic/Identity.h"
#include "platform/Win32.h"

namespace snippin {

enum class OpResult {
    Ok,
    NotCandidate,   // desktop, taskbar, our own window, invalid handle
    AccessDenied,   // blocked by UIPI (target is elevated)
    Unsupported,    // window uses per-pixel alpha (UpdateLayeredWindow)
};

struct TrackedWindow {
    HWND hwnd = nullptr;
    logic::WindowIdentity identity;
    JournalEntry original;  // state before SnipPin touched the window

    bool topmost = false;
    int opacityPercent = 100;
    bool ghost = false;
    std::uint64_t ghostSequence = 0;  // ordering for "un-ghost the last one"
    std::uint64_t pinSequence = 0;    // ordering for menus/panel
};

class WindowTracker {
public:
    explicit WindowTracker(RecoveryJournal& journal) : journal_(journal) {}

    OpResult SetTopmost(HWND hwnd, bool topmost);
    OpResult ToggleTopmost(HWND hwnd);
    OpResult SetOpacity(HWND hwnd, int percent, int minPercent);
    OpResult StepOpacity(HWND hwnd, int deltaPercent, int minPercent);
    // Ghost mode implies always-on-top. |ghostOpacityPercent| is used when the
    // window is still fully opaque, so the user can see it is a ghost.
    OpResult SetGhost(HWND hwnd, bool ghost, int ghostOpacityPercent, int minPercent);

    // Restores the original state and stops tracking.
    void Release(HWND hwnd);
    void ReleaseAll();

    // Window was destroyed: forget it without touching it.
    void Forget(HWND hwnd);

    // Re-applies HWND_TOPMOST to every pinned window. Returns how many
    // windows had lost their topmost flag.
    int ReassertTopmost(bool force);

    const TrackedWindow* Find(HWND hwnd) const;
    const std::vector<TrackedWindow>& Windows() const { return windows_; }
    HWND MostRecentGhost() const;

    // Fired after any state change (menus, stash panel and history listen).
    std::function<void(const TrackedWindow&)> onChanged;
    std::function<void()> onListChanged;

private:
    TrackedWindow* FindMutable(HWND hwnd);
    OpResult Track(HWND hwnd, TrackedWindow*& out);
    OpResult ApplyStyles(TrackedWindow& tw);
    void ReleaseIfIdle(HWND hwnd);
    void Notify(const TrackedWindow& tw);

    RecoveryJournal& journal_;
    std::vector<TrackedWindow> windows_;
    std::uint64_t sequence_ = 0;
};

}  // namespace snippin
