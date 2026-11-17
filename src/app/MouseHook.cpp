#include "app/MouseHook.h"

#include "logic/Hotkey.h"
#include "platform/Log.h"

namespace snippin {

MouseHook* MouseHook::instance_ = nullptr;

MouseHook::~MouseHook() { Uninstall(); }

bool MouseHook::Install(HINSTANCE instance, HWND host, UINT contextMenuMessage, WheelFilter wheelFilter) {
    Uninstall();
    host_ = host;
    contextMenuMessage_ = contextMenuMessage;
    wheelFilter_ = std::move(wheelFilter);
    instance_ = this;
    hook_ = ::SetWindowsHookExW(WH_MOUSE_LL, &MouseHook::HookProc, instance, 0);
    if (!hook_) {
        SNIPPIN_LOG(L"SetWindowsHookEx(WH_MOUSE_LL) failed (error %lu)", ::GetLastError());
    }
    return hook_ != nullptr;
}

void MouseHook::Uninstall() {
    if (hook_) {
        ::UnhookWindowsHookEx(hook_);
        hook_ = nullptr;
    }
    if (instance_ == this) {
        instance_ = nullptr;
    }
}

void MouseHook::Configure(bool contextMenuEnabled, unsigned modifiers) {
    contextMenuEnabled_ = contextMenuEnabled;
    modifiers_ = modifiers;
}

bool MouseHook::ModifiersMatch() const {
    auto down = [](int vk) { return (::GetAsyncKeyState(vk) & 0x8000) != 0; };
    const bool ctrl = down(VK_CONTROL);
    const bool alt = down(VK_MENU);
    const bool shift = down(VK_SHIFT);
    const bool win = down(VK_LWIN) || down(VK_RWIN);
    // Exact match, so e.g. Ctrl+Shift+right-click stays available to apps
    // when the gesture is plain Ctrl+right-click.
    return ctrl == ((modifiers_ & logic::kModControl) != 0) && alt == ((modifiers_ & logic::kModAlt) != 0) &&
           shift == ((modifiers_ & logic::kModShift) != 0) && win == ((modifiers_ & logic::kModWin) != 0);
}

LRESULT CALLBACK MouseHook::HookProc(int code, WPARAM wParam, LPARAM lParam) {
    MouseHook* self = instance_;
    if (code != HC_ACTION || !self) {
        return ::CallNextHookEx(nullptr, code, wParam, lParam);
    }
    const auto* info = reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam);
    const bool injected = (info->flags & LLMHF_INJECTED) != 0;

    switch (wParam) {
        case WM_RBUTTONDOWN:
            if (!injected && self->contextMenuEnabled_ && self->modifiers_ != 0 && self->ModifiersMatch()) {
                self->swallowingRightButton_ = true;
                return 1;  // the target app never sees this click
            }
            break;
        case WM_RBUTTONUP:
            if (self->swallowingRightButton_) {
                self->swallowingRightButton_ = false;
                ::PostMessageW(self->host_, self->contextMenuMessage_, static_cast<WPARAM>(info->pt.x),
                               static_cast<LPARAM>(info->pt.y));
                return 1;
            }
            break;
        case WM_MOUSEWHEEL:
            if (!injected && self->wheelFilter_) {
                const int delta = static_cast<short>(HIWORD(info->mouseData));
                const bool ctrl = (::GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
                if (self->wheelFilter_(info->pt, delta, ctrl)) {
                    return 1;
                }
            }
            break;
        default:
            break;
    }
    return ::CallNextHookEx(nullptr, code, wParam, lParam);
}

}  // namespace snippin
