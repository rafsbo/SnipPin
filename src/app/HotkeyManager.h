// Registers the global keyboard shortcuts (RegisterHotKey, no hooks).
#pragma once

#include <string>

#include "platform/Settings.h"
#include "platform/Win32.h"

namespace snippin {

class HotkeyManager {
public:
    // Hotkey ids are HotkeyAction index + kIdBase.
    static constexpr int kIdBase = 1;

    // Returns false if any shortcut could not be registered; |failures|
    // receives a comma-separated list of the conflicting shortcuts.
    bool RegisterAll(HWND hwnd, const AppSettings& settings, std::wstring& failures);
    void UnregisterAll();

    static bool ActionFromId(int id, HotkeyAction& action);

private:
    HWND hwnd_ = nullptr;
    bool registered_[static_cast<int>(HotkeyAction::Count)] = {};
};

}  // namespace snippin
