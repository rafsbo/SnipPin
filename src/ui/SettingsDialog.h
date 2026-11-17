// Modal settings dialog (resource IDD_SETTINGS).
#pragma once

#include "platform/Settings.h"
#include "platform/Win32.h"

namespace snippin {

struct SettingsDialogResult {
    AppSettings settings;
    bool runAtStartup = false;
};

// Returns true when the user pressed OK; |inOut| then holds the new values.
bool ShowSettingsDialog(HINSTANCE instance, HWND owner, SettingsDialogResult& inOut);

}  // namespace snippin
