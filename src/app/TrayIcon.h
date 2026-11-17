// Notification-area icon (Shell_NotifyIcon, NOTIFYICON_VERSION_4).
#pragma once

#include "platform/Win32.h"

namespace snippin {

class TrayIcon {
public:
    bool Add(HWND owner, UINT callbackMessage, HICON icon, const wchar_t* tooltip);
    void Remove();
    // Call when Explorer restarts (the "TaskbarCreated" broadcast).
    void Readd();
    void ShowBalloon(const wchar_t* title, const wchar_t* text, bool warning);

private:
    NOTIFYICONDATAW data_{};
    bool added_ = false;
};

}  // namespace snippin
