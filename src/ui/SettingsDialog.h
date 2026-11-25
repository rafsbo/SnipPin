// Modal settings dialog (resource IDD_SETTINGS).
#pragma once

#include <vector>

#include "platform/Localization.h"
#include "platform/Settings.h"
#include "platform/Win32.h"

namespace snippin {

struct SettingsDialogResult {
    AppSettings settings;
    bool runAtStartup = false;
    std::vector<l10n::LanguageInfo> languages;  // offered in the language list
};

// Returns true when the user pressed OK; |inOut| then holds the new values.
bool ShowSettingsDialog(HINSTANCE instance, HWND owner, SettingsDialogResult& inOut);

}  // namespace snippin
