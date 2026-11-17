#include "core/ZOrderGuard.h"

#include "platform/Log.h"

namespace snippin {

ZOrderGuard* ZOrderGuard::instance_ = nullptr;

ZOrderGuard::~ZOrderGuard() { Stop(); }

bool ZOrderGuard::Start(HWND host, WindowEventHandler onWindowEvent, ReassertHandler onReassert) {
    Stop();
    host_ = host;
    onWindowEvent_ = std::move(onWindowEvent);
    onReassert_ = std::move(onReassert);
    instance_ = this;

    const DWORD flags = WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS;
    foregroundHook_ = ::SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr,
                                        &ZOrderGuard::WinEventProc, 0, 0, flags);
    minimizeHook_ = ::SetWinEventHook(EVENT_SYSTEM_MINIMIZESTART, EVENT_SYSTEM_MINIMIZEEND, nullptr,
                                      &ZOrderGuard::WinEventProc, 0, 0, flags);
    // DESTROY (0x8001) .. SHOW (0x8002): window lifetime tracking.
    objectHook_ = ::SetWinEventHook(EVENT_OBJECT_DESTROY, EVENT_OBJECT_SHOW, nullptr,
                                    &ZOrderGuard::WinEventProc, 0, 0, flags);
    if (!foregroundHook_ || !objectHook_) {
        SNIPPIN_LOG(L"SetWinEventHook failed (error %lu)", ::GetLastError());
    }
    SetEnabled(enabled_, intervalMs_);
    return foregroundHook_ != nullptr;
}

void ZOrderGuard::Stop() {
    for (HWINEVENTHOOK* hook : {&foregroundHook_, &minimizeHook_, &objectHook_}) {
        if (*hook) {
            ::UnhookWinEvent(*hook);
            *hook = nullptr;
        }
    }
    if (host_) {
        ::KillTimer(host_, kDebounceTimer);
        ::KillTimer(host_, kPeriodicTimer);
    }
    if (instance_ == this) {
        instance_ = nullptr;
    }
}

void ZOrderGuard::SetEnabled(bool enabled, int intervalMs) {
    enabled_ = enabled;
    intervalMs_ = intervalMs;
    if (!host_) {
        return;
    }
    if (enabled_) {
        ::SetTimer(host_, kPeriodicTimer, static_cast<UINT>(intervalMs_), nullptr);
    } else {
        ::KillTimer(host_, kPeriodicTimer);
        ::KillTimer(host_, kDebounceTimer);
    }
}

void CALLBACK ZOrderGuard::WinEventProc(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG idObject, LONG idChild,
                                        DWORD, DWORD) {
    // Only whole-window events matter (not carets, cursors, child objects).
    if (!instance_ || !hwnd || idObject != OBJID_WINDOW || idChild != CHILDID_SELF) {
        return;
    }
    instance_->HandleEvent(event, hwnd);
}

void ZOrderGuard::HandleEvent(DWORD event, HWND hwnd) {
    if (onWindowEvent_) {
        onWindowEvent_(event, hwnd);
    }
    if (event == EVENT_SYSTEM_FOREGROUND || event == EVENT_SYSTEM_MINIMIZEEND) {
        RequestReassert();
    }
}

void ZOrderGuard::RequestReassert() {
    if (enabled_ && host_) {
        // Short delay lets the new foreground window finish raising itself.
        ::SetTimer(host_, kDebounceTimer, 60, nullptr);
    }
}

bool ZOrderGuard::AllowReassert() {
    const std::uint64_t now = ::GetTickCount64();
    if (now - rateWindowStart_ > kWindowMs) {
        rateWindowStart_ = now;
        rateCount_ = 0;
    }
    if (rateCount_ >= kMaxPerWindow) {
        return false;
    }
    ++rateCount_;
    return true;
}

bool ZOrderGuard::OnTimer(UINT_PTR timerId) {
    if (timerId == kDebounceTimer) {
        ::KillTimer(host_, kDebounceTimer);
        if (enabled_ && onReassert_ && AllowReassert()) {
            onReassert_(true);
        }
        return true;
    }
    if (timerId == kPeriodicTimer) {
        if (enabled_ && onReassert_) {
            onReassert_(false);
        }
        return true;
    }
    return false;
}

}  // namespace snippin
