// Persistent configuration stored in UTF-16 INI files.
//   Installed mode: %APPDATA%\SnipPin\settings.ini
//   Portable mode:  <exe folder>\settings.ini (enabled by a portable.ini file)
#pragma once

#include <string>

#include "logic/Hotkey.h"
#include "platform/Win32.h"

namespace snippin {

// Thin wrapper around the Win32 profile API that keeps files Unicode.
class IniFile {
public:
    IniFile() = default;
    explicit IniFile(std::wstring path) : path_(std::move(path)) {}

    const std::wstring& Path() const { return path_; }

    // Creates the file with a UTF-16LE BOM so non-ASCII text round-trips.
    void EnsureUnicode() const;

    std::wstring GetString(const wchar_t* section, const wchar_t* key, const wchar_t* fallback = L"") const;
    int GetInt(const wchar_t* section, const wchar_t* key, int fallback) const;
    bool GetBool(const wchar_t* section, const wchar_t* key, bool fallback) const;

    void SetString(const wchar_t* section, const wchar_t* key, const std::wstring& value) const;
    void SetInt(const wchar_t* section, const wchar_t* key, int value) const;
    void SetBool(const wchar_t* section, const wchar_t* key, bool value) const;

    void DeleteSection(const wchar_t* section) const;
    void DeleteKey(const wchar_t* section, const wchar_t* key) const;
    void Flush() const;

private:
    std::wstring path_;
};

enum class HotkeyAction {
    ToggleTopmost,
    PickWindow,
    OpacityDown,
    OpacityUp,
    ToggleGhost,
    SnipRegion,
    ToggleStash,
    PanicRestore,
    Count
};

struct AppSettings {
    logic::Hotkey hotkeys[static_cast<int>(HotkeyAction::Count)];

    int opacityStepPercent = 10;
    int minOpacityPercent = 20;
    int ghostOpacityPercent = 60;  // applied when ghost mode starts at 100 %

    bool contextMenuEnabled = true;
    unsigned contextMenuModifiers = logic::kModControl;  // modifier(s) + right-click

    bool zOrderGuardEnabled = true;
    int zOrderGuardIntervalMs = 1000;

    bool showNotifications = true;
    bool snippetCompatibilityMode = false;  // force PrintWindow renderer
    int snippetFallbackIntervalMs = 250;
    bool snippetLiveByDefault = true;

    int historySize = 20;
    std::wstring language = L"auto";  // "auto" (Windows display language) or a code such as "it"
    bool firstRunDone = false;
    bool logToFile = false;

    static AppSettings Defaults();
    static const wchar_t* HotkeyKeyName(HotkeyAction action);
};

class SettingsStore {
public:
    // Resolves the data folder (portable or %APPDATA%) and creates it.
    bool Initialize();

    const std::wstring& DataDirectory() const { return dataDir_; }
    bool IsPortable() const { return portable_; }
    std::wstring FilePath(const wchar_t* fileName) const;

    AppSettings Load() const;
    void Save(const AppSettings& settings) const;

    // Start with Windows (HKCU\...\Run). "Enabled" means Windows will really
    // start SnipPin: the Run value exists and it was not switched off in Task
    // Manager's Startup page (StartupApproved, Windows 8+).
    static bool IsRunAtStartupEnabled();
    static bool SetRunAtStartup(bool enabled);
    // If start-up is enabled but points to an executable that no longer
    // exists (SnipPin was moved), re-register the current executable.
    static void RepairRunAtStartupPath();
    // Command line written to the Run key: "<exe path>" --autostart
    static std::wstring RunCommandLine();

private:
    std::wstring dataDir_;
    bool portable_ = false;
};

}  // namespace snippin
