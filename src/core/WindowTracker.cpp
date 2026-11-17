#include "core/WindowTracker.h"

#include <algorithm>

#include "logic/Geometry.h"
#include "platform/Log.h"
#include "platform/WinUtil.h"

namespace snippin {
namespace {

constexpr UINT kTopmostFlags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER;

bool HasTopmostStyle(HWND hwnd) {
    return (::GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
}

}  // namespace

TrackedWindow* WindowTracker::FindMutable(HWND hwnd) {
    for (auto& tw : windows_) {
        if (tw.hwnd == hwnd) {
            return &tw;
        }
    }
    return nullptr;
}

const TrackedWindow* WindowTracker::Find(HWND hwnd) const {
    for (const auto& tw : windows_) {
        if (tw.hwnd == hwnd) {
            return &tw;
        }
    }
    return nullptr;
}

HWND WindowTracker::MostRecentGhost() const {
    const TrackedWindow* best = nullptr;
    for (const auto& tw : windows_) {
        if (tw.ghost && (!best || tw.ghostSequence > best->ghostSequence)) {
            best = &tw;
        }
    }
    return best ? best->hwnd : nullptr;
}

void WindowTracker::Notify(const TrackedWindow& tw) {
    if (onChanged) {
        onChanged(tw);
    }
    if (onListChanged) {
        onListChanged();
    }
}

OpResult WindowTracker::Track(HWND hwnd, TrackedWindow*& out) {
    out = FindMutable(hwnd);
    if (out) {
        return OpResult::Ok;
    }
    if (!win::IsCandidateWindow(hwnd)) {
        return OpResult::NotCandidate;
    }
    TrackedWindow tw;
    tw.hwnd = hwnd;
    tw.identity = win::GetIdentity(hwnd);
    tw.pinSequence = ++sequence_;

    JournalEntry& o = tw.original;
    o.hwnd = hwnd;
    o.pid = win::GetWindowProcessId(hwnd);
    o.originalExStyle = ::GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    o.originallyTopmost = (o.originalExStyle & WS_EX_TOPMOST) != 0;
    if (o.originalExStyle & WS_EX_LAYERED) {
        BYTE alpha = 255;
        DWORD flags = 0;
        COLORREF key = 0;
        // Fails for windows driven by UpdateLayeredWindow (per-pixel alpha).
        o.layeredAttrsValid = ::GetLayeredWindowAttributes(hwnd, &key, &alpha, &flags) != FALSE;
        o.originalAlpha = alpha;
        o.originalLwaFlags = flags;
        o.originalColorKey = key;
        if (o.layeredAttrsValid && (flags & LWA_ALPHA)) {
            tw.opacityPercent = logic::AlphaToPercent(alpha);
        }
    }
    windows_.push_back(std::move(tw));
    out = &windows_.back();
    return OpResult::Ok;
}

OpResult WindowTracker::ApplyStyles(TrackedWindow& tw) {
    const JournalEntry& o = tw.original;
    const bool originallyLayered = (o.originalExStyle & WS_EX_LAYERED) != 0;
    const bool needLayered = tw.ghost || tw.opacityPercent < 100;

    if (needLayered && originallyLayered && !o.layeredAttrsValid) {
        return OpResult::Unsupported;  // per-pixel alpha window, leave it alone
    }

    journal_.Record(o);

    const LONG_PTR current = ::GetWindowLongPtrW(tw.hwnd, GWL_EXSTYLE);
    LONG_PTR desired = current;
    if (needLayered || originallyLayered) {
        desired |= WS_EX_LAYERED;
    } else {
        desired &= ~static_cast<LONG_PTR>(WS_EX_LAYERED);
    }
    if (tw.ghost) {
        desired |= WS_EX_TRANSPARENT;
    } else {
        desired = (desired & ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT)) | (o.originalExStyle & WS_EX_TRANSPARENT);
    }

    if (desired != current) {
        ::SetLastError(0);
        ::SetWindowLongPtrW(tw.hwnd, GWL_EXSTYLE, desired);
        const LONG_PTR applied = ::GetWindowLongPtrW(tw.hwnd, GWL_EXSTYLE);
        if ((applied & (WS_EX_LAYERED | WS_EX_TRANSPARENT)) != (desired & (WS_EX_LAYERED | WS_EX_TRANSPARENT))) {
            SNIPPIN_LOG(L"SetWindowLongPtr refused for %p (error %lu)", tw.hwnd, ::GetLastError());
            return OpResult::AccessDenied;
        }
    }

    if (needLayered) {
        DWORD flags = LWA_ALPHA;
        COLORREF key = 0;
        if (originallyLayered && (o.originalLwaFlags & LWA_COLORKEY)) {
            flags |= LWA_COLORKEY;  // preserve an app's own colour key
            key = o.originalColorKey;
        }
        if (!::SetLayeredWindowAttributes(tw.hwnd, key, logic::PercentToAlpha(tw.opacityPercent), flags)) {
            SNIPPIN_LOG(L"SetLayeredWindowAttributes failed for %p (error %lu)", tw.hwnd, ::GetLastError());
            return OpResult::Unsupported;
        }
    } else if (originallyLayered && o.layeredAttrsValid) {
        ::SetLayeredWindowAttributes(tw.hwnd, o.originalColorKey, o.originalAlpha, o.originalLwaFlags);
    } else if (desired != current) {
        // Removing WS_EX_LAYERED can leave stale pixels on some apps.
        ::RedrawWindow(tw.hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN | RDW_ERASE);
    }
    return OpResult::Ok;
}

OpResult WindowTracker::SetTopmost(HWND hwnd, bool topmost) {
    TrackedWindow* tw = nullptr;
    OpResult r = Track(hwnd, tw);
    if (r != OpResult::Ok) {
        return r;
    }
    tw->original.topmostTouched = true;
    journal_.Record(tw->original);

    ::SetWindowPos(hwnd, topmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, kTopmostFlags);
    if (HasTopmostStyle(hwnd) != topmost) {
        SNIPPIN_LOG(L"SetWindowPos(topmost=%d) refused for %p", topmost ? 1 : 0, hwnd);
        if (!tw->topmost && !tw->ghost && tw->opacityPercent == 100) {
            Release(hwnd);
        }
        return OpResult::AccessDenied;
    }
    tw->topmost = topmost;
    if (!topmost && tw->ghost) {
        tw->ghost = false;  // a ghost that falls behind other windows is useless
        ApplyStyles(*tw);
    }
    Notify(*tw);
    ReleaseIfIdle(hwnd);
    return OpResult::Ok;
}

OpResult WindowTracker::ToggleTopmost(HWND hwnd) {
    const TrackedWindow* tw = Find(hwnd);
    return SetTopmost(hwnd, !(tw && tw->topmost));
}

OpResult WindowTracker::SetOpacity(HWND hwnd, int percent, int minPercent) {
    TrackedWindow* tw = nullptr;
    OpResult r = Track(hwnd, tw);
    if (r != OpResult::Ok) {
        return r;
    }
    const int previous = tw->opacityPercent;
    tw->opacityPercent = logic::ClampOpacityPercent(percent, minPercent);
    r = ApplyStyles(*tw);
    if (r != OpResult::Ok) {
        tw->opacityPercent = previous;
        ReleaseIfIdle(hwnd);
        return r;
    }
    Notify(*tw);
    ReleaseIfIdle(hwnd);
    return OpResult::Ok;
}

OpResult WindowTracker::StepOpacity(HWND hwnd, int deltaPercent, int minPercent) {
    const TrackedWindow* tw = Find(hwnd);
    const int current = tw ? tw->opacityPercent : 100;
    return SetOpacity(hwnd, logic::StepOpacityPercent(current, deltaPercent, minPercent), minPercent);
}

OpResult WindowTracker::SetGhost(HWND hwnd, bool ghost, int ghostOpacityPercent, int minPercent) {
    TrackedWindow* tw = nullptr;
    OpResult r = Track(hwnd, tw);
    if (r != OpResult::Ok) {
        return r;
    }
    const TrackedWindow backup = *tw;
    tw->ghost = ghost;
    if (ghost) {
        tw->ghostSequence = ++sequence_;
        if (tw->opacityPercent >= 100) {
            tw->opacityPercent = logic::ClampOpacityPercent(ghostOpacityPercent, minPercent);
        }
    }
    r = ApplyStyles(*tw);
    if (r != OpResult::Ok) {
        *tw = backup;
        ApplyStyles(*tw);
        ReleaseIfIdle(hwnd);
        return r;
    }
    if (ghost && !tw->topmost) {
        // Ghost windows must float above the app being worked on.
        const OpResult t = SetTopmost(hwnd, true);
        if (t != OpResult::Ok) {
            if (TrackedWindow* again = FindMutable(hwnd)) {
                again->ghost = false;
                again->opacityPercent = backup.opacityPercent;
                ApplyStyles(*again);
                ReleaseIfIdle(hwnd);
            }
            return t;
        }
        return OpResult::Ok;  // SetTopmost already notified
    }
    Notify(*tw);
    ReleaseIfIdle(hwnd);
    return OpResult::Ok;
}

void WindowTracker::ReleaseIfIdle(HWND hwnd) {
    const TrackedWindow* tw = Find(hwnd);
    if (tw && !tw->topmost && !tw->ghost && tw->opacityPercent >= 100) {
        Release(hwnd);
    }
}

void WindowTracker::Release(HWND hwnd) {
    auto it = std::find_if(windows_.begin(), windows_.end(),
                           [hwnd](const TrackedWindow& tw) { return tw.hwnd == hwnd; });
    if (it == windows_.end()) {
        return;
    }
    RecoveryJournal::RestoreWindow(it->original);
    journal_.Remove(hwnd);
    windows_.erase(it);
    if (onListChanged) {
        onListChanged();
    }
}

void WindowTracker::ReleaseAll() {
    for (const auto& tw : windows_) {
        RecoveryJournal::RestoreWindow(tw.original);
        journal_.Remove(tw.hwnd);
    }
    windows_.clear();
    if (onListChanged) {
        onListChanged();
    }
}

void WindowTracker::Forget(HWND hwnd) {
    auto it = std::find_if(windows_.begin(), windows_.end(),
                           [hwnd](const TrackedWindow& tw) { return tw.hwnd == hwnd; });
    if (it == windows_.end()) {
        return;
    }
    journal_.Remove(hwnd);
    windows_.erase(it);
    if (onListChanged) {
        onListChanged();
    }
}

int WindowTracker::ReassertTopmost(bool force) {
    int lost = 0;
    // Iterate a copy: a vanished window is forgotten while looping.
    const auto snapshot = windows_;
    for (const auto& tw : snapshot) {
        if (!::IsWindow(tw.hwnd)) {
            Forget(tw.hwnd);
            continue;
        }
        if (!tw.topmost) {
            continue;
        }
        const bool missing = !HasTopmostStyle(tw.hwnd);
        if (missing) {
            ++lost;
        }
        if (missing || force) {
            // Re-inserting at HWND_TOPMOST also moves it to the top of the
            // topmost band, above fullscreen windows that are topmost too.
            ::SetWindowPos(tw.hwnd, HWND_TOPMOST, 0, 0, 0, 0, kTopmostFlags);
        }
    }
    return lost;
}

}  // namespace snippin
