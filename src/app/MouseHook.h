// Low-level mouse hook (WH_MOUSE_LL). Runs inside SnipPin's own process - no
// DLL is injected into other applications - and is used for two things:
//   1. The configurable "modifier + right-click" gesture that opens the
//      SnipPin context menu for the window under the cursor.
//   2. Routing the mouse wheel to snippets on Windows 7/8, where inactive
//      windows do not receive wheel input.
// The callback must return quickly (Windows silently removes slow hooks), so
// it only posts messages to the host window.
#pragma once

#include <functional>

#include "platform/Win32.h"

namespace snippin {

class MouseHook {
public:
    // Returns true when the wheel event at |pt| was consumed.
    using WheelFilter = std::function<bool(POINT pt, int delta, bool ctrl)>;

    ~MouseHook();

    bool Install(HINSTANCE instance, HWND host, UINT contextMenuMessage, WheelFilter wheelFilter);
    void Uninstall();
    void Configure(bool contextMenuEnabled, unsigned modifiers);

private:
    static LRESULT CALLBACK HookProc(int code, WPARAM wParam, LPARAM lParam);
    bool ModifiersMatch() const;

    HHOOK hook_ = nullptr;
    HWND host_ = nullptr;
    UINT contextMenuMessage_ = 0;
    WheelFilter wheelFilter_;
    bool contextMenuEnabled_ = true;
    unsigned modifiers_ = 0;
    bool swallowingRightButton_ = false;

    static MouseHook* instance_;
};

}  // namespace snippin
