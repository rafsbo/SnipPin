#include "app/HotkeyManager.h"

#include "platform/Log.h"

#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000  // Windows 7+
#endif

namespace snippin {

bool HotkeyManager::RegisterAll(HWND hwnd, const AppSettings& settings, std::wstring& failures) {
    UnregisterAll();
    hwnd_ = hwnd;
    failures.clear();
    for (int i = 0; i < static_cast<int>(HotkeyAction::Count); ++i) {
        const logic::Hotkey& hk = settings.hotkeys[i];
        if (!hk.IsEnabled()) {
            continue;
        }
        if (::RegisterHotKey(hwnd, kIdBase + i, hk.modifiers | MOD_NOREPEAT, hk.vk)) {
            registered_[i] = true;
        } else {
            SNIPPIN_LOG(L"RegisterHotKey failed for %s (error %lu)", logic::FormatHotkey(hk).c_str(),
                        ::GetLastError());
            if (!failures.empty()) {
                failures += L", ";
            }
            failures += logic::FormatHotkey(hk);
        }
    }
    return failures.empty();
}

void HotkeyManager::UnregisterAll() {
    for (int i = 0; i < static_cast<int>(HotkeyAction::Count); ++i) {
        if (registered_[i]) {
            ::UnregisterHotKey(hwnd_, kIdBase + i);
            registered_[i] = false;
        }
    }
}

bool HotkeyManager::ActionFromId(int id, HotkeyAction& action) {
    const int index = id - kIdBase;
    if (index < 0 || index >= static_cast<int>(HotkeyAction::Count)) {
        return false;
    }
    action = static_cast<HotkeyAction>(index);
    return true;
}

}  // namespace snippin
