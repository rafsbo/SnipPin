#include "app/TrayIcon.h"

#include <cwchar>

namespace snippin {

bool TrayIcon::Add(HWND owner, UINT callbackMessage, HICON icon, const wchar_t* tooltip) {
    data_ = NOTIFYICONDATAW{};
    data_.cbSize = sizeof(data_);
    data_.hWnd = owner;
    data_.uID = 1;  // id-based (not GUID) so moving the exe keeps working
    data_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    data_.uCallbackMessage = callbackMessage;
    data_.hIcon = icon;
    wcsncpy_s(data_.szTip, tooltip, _TRUNCATE);
    added_ = ::Shell_NotifyIconW(NIM_ADD, &data_) != FALSE;
    if (added_) {
        data_.uVersion = NOTIFYICON_VERSION_4;
        ::Shell_NotifyIconW(NIM_SETVERSION, &data_);
    }
    return added_;
}

void TrayIcon::Remove() {
    if (added_) {
        ::Shell_NotifyIconW(NIM_DELETE, &data_);
        added_ = false;
    }
}

void TrayIcon::Readd() {
    added_ = false;
    NOTIFYICONDATAW copy = data_;
    copy.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    added_ = ::Shell_NotifyIconW(NIM_ADD, &copy) != FALSE;
    if (added_) {
        copy.uVersion = NOTIFYICON_VERSION_4;
        ::Shell_NotifyIconW(NIM_SETVERSION, &copy);
    }
}

void TrayIcon::ShowBalloon(const wchar_t* title, const wchar_t* text, bool warning) {
    if (!added_) {
        return;
    }
    NOTIFYICONDATAW info = data_;
    info.uFlags = NIF_INFO;
    info.dwInfoFlags = (warning ? NIIF_WARNING : NIIF_INFO) | NIIF_RESPECT_QUIET_TIME;
    wcsncpy_s(info.szInfoTitle, title, _TRUNCATE);
    wcsncpy_s(info.szInfo, text, _TRUNCATE);
    ::Shell_NotifyIconW(NIM_MODIFY, &info);
}

}  // namespace snippin
