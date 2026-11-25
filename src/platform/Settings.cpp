#include "platform/Settings.h"

#include <shlobj.h>

#include <algorithm>
#include <cwchar>
#include <vector>

#include "platform/WinUtil.h"

namespace snippin {
namespace {

constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValue[] = L"SnipPin";
// Task Manager's "Startup apps" switch (Windows 8+). The first byte of the
// 12-byte binary value is even (0x02/0x06) when enabled, odd when disabled.
constexpr wchar_t kStartupApprovedKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";

constexpr wchar_t kSecHotkeys[] = L"Hotkeys";
constexpr wchar_t kSecGeneral[] = L"General";
constexpr wchar_t kSecSnippets[] = L"Snippets";
constexpr wchar_t kSecContextMenu[] = L"ContextMenu";
constexpr wchar_t kSecDebug[] = L"Debug";

bool FileExists(const std::wstring& path) {
    const DWORD attrs = ::GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

}  // namespace

// ---------------------------------------------------------------- IniFile

void IniFile::EnsureUnicode() const {
    if (path_.empty() || FileExists(path_)) {
        return;
    }
    HANDLE file = ::CreateFileW(path_.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        const unsigned char bom[] = {0xFF, 0xFE};
        DWORD written = 0;
        ::WriteFile(file, bom, sizeof(bom), &written, nullptr);
        ::CloseHandle(file);
    }
}

std::wstring IniFile::GetString(const wchar_t* section, const wchar_t* key, const wchar_t* fallback) const {
    std::vector<wchar_t> buffer(1024);
    for (;;) {
        const DWORD len = ::GetPrivateProfileStringW(section, key, fallback, buffer.data(),
                                                     static_cast<DWORD>(buffer.size()), path_.c_str());
        if (len < buffer.size() - 1) {
            return std::wstring(buffer.data(), len);
        }
        if (buffer.size() >= 65536) {
            return std::wstring(buffer.data(), len);
        }
        buffer.resize(buffer.size() * 4);
    }
}

int IniFile::GetInt(const wchar_t* section, const wchar_t* key, int fallback) const {
    const std::wstring value = GetString(section, key, L"");
    if (value.empty()) {
        return fallback;
    }
    wchar_t* end = nullptr;
    const long parsed = std::wcstol(value.c_str(), &end, 10);
    return (end && *end == L'\0') ? static_cast<int>(parsed) : fallback;
}

bool IniFile::GetBool(const wchar_t* section, const wchar_t* key, bool fallback) const {
    const std::wstring value = GetString(section, key, L"");
    if (value.empty()) {
        return fallback;
    }
    return value == L"1" || value == L"true" || value == L"True" || value == L"yes";
}

void IniFile::SetString(const wchar_t* section, const wchar_t* key, const std::wstring& value) const {
    EnsureUnicode();
    ::WritePrivateProfileStringW(section, key, value.c_str(), path_.c_str());
}

void IniFile::SetInt(const wchar_t* section, const wchar_t* key, int value) const {
    SetString(section, key, std::to_wstring(value));
}

void IniFile::SetBool(const wchar_t* section, const wchar_t* key, bool value) const {
    SetString(section, key, value ? L"1" : L"0");
}

void IniFile::DeleteSection(const wchar_t* section) const {
    ::WritePrivateProfileStringW(section, nullptr, nullptr, path_.c_str());
}

void IniFile::DeleteKey(const wchar_t* section, const wchar_t* key) const {
    ::WritePrivateProfileStringW(section, key, nullptr, path_.c_str());
}

void IniFile::Flush() const {
    ::WritePrivateProfileStringW(nullptr, nullptr, nullptr, path_.c_str());
}

// ---------------------------------------------------------------- AppSettings

AppSettings AppSettings::Defaults() {
    using namespace logic;
    AppSettings s;
    auto set = [&s](HotkeyAction a, unsigned mods, unsigned vk) {
        s.hotkeys[static_cast<int>(a)] = Hotkey{mods, vk};
    };
    const unsigned ctrlAlt = kModControl | kModAlt;
    set(HotkeyAction::ToggleTopmost, ctrlAlt, 'T');
    set(HotkeyAction::PickWindow, ctrlAlt, 'P');
    set(HotkeyAction::OpacityDown, ctrlAlt, VK_DOWN);
    set(HotkeyAction::OpacityUp, ctrlAlt, VK_UP);
    set(HotkeyAction::ToggleGhost, ctrlAlt, 'G');
    set(HotkeyAction::SnipRegion, ctrlAlt, 'S');
    set(HotkeyAction::ToggleStash, ctrlAlt, 'H');
    set(HotkeyAction::PanicRestore, ctrlAlt, VK_BACK);
    return s;
}

const wchar_t* AppSettings::HotkeyKeyName(HotkeyAction action) {
    switch (action) {
        case HotkeyAction::ToggleTopmost: return L"ToggleTopmost";
        case HotkeyAction::PickWindow: return L"PickWindow";
        case HotkeyAction::OpacityDown: return L"OpacityDown";
        case HotkeyAction::OpacityUp: return L"OpacityUp";
        case HotkeyAction::ToggleGhost: return L"ToggleGhost";
        case HotkeyAction::SnipRegion: return L"SnipRegion";
        case HotkeyAction::ToggleStash: return L"ToggleStash";
        case HotkeyAction::PanicRestore: return L"PanicRestore";
        default: return L"";
    }
}

// ---------------------------------------------------------------- SettingsStore

bool SettingsStore::Initialize() {
    const std::wstring exeDir = win::GetModuleDirectory();
    if (FileExists(exeDir + L"\\portable.ini")) {
        portable_ = true;
        dataDir_ = exeDir;
        return true;
    }
    PWSTR appData = nullptr;
    if (FAILED(::SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE, nullptr, &appData))) {
        portable_ = true;
        dataDir_ = exeDir;
        return true;
    }
    dataDir_ = std::wstring(appData) + L"\\SnipPin";
    ::CoTaskMemFree(appData);
    if (!::CreateDirectoryW(dataDir_.c_str(), nullptr) && ::GetLastError() != ERROR_ALREADY_EXISTS) {
        portable_ = true;
        dataDir_ = exeDir;
    }
    return true;
}

std::wstring SettingsStore::FilePath(const wchar_t* fileName) const {
    return dataDir_ + L"\\" + fileName;
}

AppSettings SettingsStore::Load() const {
    AppSettings s = AppSettings::Defaults();
    IniFile ini(FilePath(L"settings.ini"));

    for (int i = 0; i < static_cast<int>(HotkeyAction::Count); ++i) {
        const auto action = static_cast<HotkeyAction>(i);
        const std::wstring text = ini.GetString(kSecHotkeys, AppSettings::HotkeyKeyName(action), L"\x01");
        logic::Hotkey parsed;
        if (text != L"\x01" && logic::ParseHotkey(text, parsed)) {
            s.hotkeys[i] = parsed;
        }
    }

    s.opacityStepPercent = std::clamp(ini.GetInt(kSecGeneral, L"OpacityStepPercent", s.opacityStepPercent), 1, 50);
    s.minOpacityPercent = std::clamp(ini.GetInt(kSecGeneral, L"MinOpacityPercent", s.minOpacityPercent), 5, 100);
    s.ghostOpacityPercent = std::clamp(ini.GetInt(kSecGeneral, L"GhostOpacityPercent", s.ghostOpacityPercent),
                                       s.minOpacityPercent, 100);
    s.zOrderGuardEnabled = ini.GetBool(kSecGeneral, L"ZOrderGuard", s.zOrderGuardEnabled);
    s.zOrderGuardIntervalMs = std::clamp(ini.GetInt(kSecGeneral, L"ZOrderGuardIntervalMs", s.zOrderGuardIntervalMs), 250, 10000);
    s.showNotifications = ini.GetBool(kSecGeneral, L"ShowNotifications", s.showNotifications);
    s.historySize = std::clamp(ini.GetInt(kSecGeneral, L"HistorySize", s.historySize), 0, 100);
    s.firstRunDone = ini.GetBool(kSecGeneral, L"FirstRunDone", s.firstRunDone);
    s.language = ini.GetString(kSecGeneral, L"Language", L"auto");
    if (s.language.empty() || s.language.size() > 16) {
        s.language = L"auto";
    }

    s.contextMenuEnabled = ini.GetBool(kSecContextMenu, L"Enabled", s.contextMenuEnabled);
    unsigned mods = 0;
    if (logic::ParseModifiers(ini.GetString(kSecContextMenu, L"Modifiers", L"Ctrl"), mods) && mods != 0) {
        s.contextMenuModifiers = mods;
    }

    s.snippetCompatibilityMode = ini.GetBool(kSecSnippets, L"CompatibilityMode", s.snippetCompatibilityMode);
    s.snippetFallbackIntervalMs =
        std::clamp(ini.GetInt(kSecSnippets, L"FallbackIntervalMs", s.snippetFallbackIntervalMs), 50, 5000);
    s.snippetLiveByDefault = ini.GetBool(kSecSnippets, L"LiveByDefault", s.snippetLiveByDefault);

    s.logToFile = ini.GetBool(kSecDebug, L"LogToFile", s.logToFile);
    return s;
}

void SettingsStore::Save(const AppSettings& s) const {
    IniFile ini(FilePath(L"settings.ini"));
    ini.EnsureUnicode();
    for (int i = 0; i < static_cast<int>(HotkeyAction::Count); ++i) {
        ini.SetString(kSecHotkeys, AppSettings::HotkeyKeyName(static_cast<HotkeyAction>(i)),
                      logic::FormatHotkey(s.hotkeys[i]));
    }
    ini.SetInt(kSecGeneral, L"OpacityStepPercent", s.opacityStepPercent);
    ini.SetInt(kSecGeneral, L"MinOpacityPercent", s.minOpacityPercent);
    ini.SetInt(kSecGeneral, L"GhostOpacityPercent", s.ghostOpacityPercent);
    ini.SetBool(kSecGeneral, L"ZOrderGuard", s.zOrderGuardEnabled);
    ini.SetInt(kSecGeneral, L"ZOrderGuardIntervalMs", s.zOrderGuardIntervalMs);
    ini.SetBool(kSecGeneral, L"ShowNotifications", s.showNotifications);
    ini.SetInt(kSecGeneral, L"HistorySize", s.historySize);
    ini.SetBool(kSecGeneral, L"FirstRunDone", s.firstRunDone);
    ini.SetString(kSecGeneral, L"Language", s.language);
    ini.SetBool(kSecContextMenu, L"Enabled", s.contextMenuEnabled);
    ini.SetString(kSecContextMenu, L"Modifiers", logic::FormatModifiers(s.contextMenuModifiers));
    ini.SetBool(kSecSnippets, L"CompatibilityMode", s.snippetCompatibilityMode);
    ini.SetInt(kSecSnippets, L"FallbackIntervalMs", s.snippetFallbackIntervalMs);
    ini.SetBool(kSecSnippets, L"LiveByDefault", s.snippetLiveByDefault);
    ini.SetBool(kSecDebug, L"LogToFile", s.logToFile);
    ini.Flush();
}

namespace {

std::wstring ReadRunValue() {
    HKEY key = nullptr;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return {};
    }
    wchar_t buffer[2048] = {};
    DWORD size = sizeof(buffer) - sizeof(wchar_t);
    DWORD type = 0;
    std::wstring value;
    if (::RegQueryValueExW(key, kRunValue, nullptr, &type, reinterpret_cast<BYTE*>(buffer), &size) == ERROR_SUCCESS &&
        (type == REG_SZ || type == REG_EXPAND_SZ)) {
        value = buffer;
    }
    ::RegCloseKey(key);
    return value;
}

// True when Task Manager (or Settings > Apps > Startup) disabled the entry.
bool IsDisabledInTaskManager() {
    HKEY key = nullptr;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, kStartupApprovedKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return false;
    }
    BYTE data[16] = {};
    DWORD size = sizeof(data);
    DWORD type = 0;
    const bool found = ::RegQueryValueExW(key, kRunValue, nullptr, &type, data, &size) == ERROR_SUCCESS &&
                       type == REG_BINARY && size >= 1;
    ::RegCloseKey(key);
    return found && (data[0] & 0x01) != 0;
}

void SetTaskManagerApproval(bool enabled) {
    HKEY key = nullptr;
    if (::RegCreateKeyExW(HKEY_CURRENT_USER, kStartupApprovedKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key,
                          nullptr) != ERROR_SUCCESS) {
        return;
    }
    if (enabled) {
        const BYTE approved[12] = {0x02};  // "enabled", no timestamp
        ::RegSetValueExW(key, kRunValue, 0, REG_BINARY, approved, sizeof(approved));
    } else {
        ::RegDeleteValueW(key, kRunValue);
    }
    ::RegCloseKey(key);
}

// Executable path from a command line such as "C:\x\SnipPin.exe" --autostart
std::wstring ExecutableFromCommand(const std::wstring& command) {
    if (!command.empty() && command[0] == L'"') {
        const size_t end = command.find(L'"', 1);
        return end == std::wstring::npos ? command.substr(1) : command.substr(1, end - 1);
    }
    const size_t space = command.find(L' ');
    return space == std::wstring::npos ? command : command.substr(0, space);
}

}  // namespace

std::wstring SettingsStore::RunCommandLine() {
    return L"\"" + win::GetModulePath() + L"\" --autostart";
}

bool SettingsStore::IsRunAtStartupEnabled() {
    return !ReadRunValue().empty() && !IsDisabledInTaskManager();
}

bool SettingsStore::SetRunAtStartup(bool enabled) {
    HKEY key = nullptr;
    if (::RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key,
                          nullptr) != ERROR_SUCCESS) {
        return false;
    }
    LONG result;
    if (enabled) {
        const std::wstring command = RunCommandLine();
        result = ::RegSetValueExW(key, kRunValue, 0, REG_SZ, reinterpret_cast<const BYTE*>(command.c_str()),
                                  static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    } else {
        result = ::RegDeleteValueW(key, kRunValue);
        if (result == ERROR_FILE_NOT_FOUND) {
            result = ERROR_SUCCESS;
        }
    }
    ::RegCloseKey(key);
    if (result == ERROR_SUCCESS) {
        // An explicit choice in SnipPin overrides an earlier Task Manager switch.
        SetTaskManagerApproval(enabled);
    }
    return result == ERROR_SUCCESS && IsRunAtStartupEnabled() == enabled;
}

void SettingsStore::RepairRunAtStartupPath() {
    const std::wstring command = ReadRunValue();
    if (command.empty() || command == RunCommandLine()) {
        return;
    }
    const std::wstring exe = ExecutableFromCommand(command);
    if (::GetFileAttributesW(exe.c_str()) == INVALID_FILE_ATTRIBUTES) {
        HKEY key = nullptr;
        if (::RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &key) == ERROR_SUCCESS) {
            const std::wstring fixed = RunCommandLine();
            ::RegSetValueExW(key, kRunValue, 0, REG_SZ, reinterpret_cast<const BYTE*>(fixed.c_str()),
                             static_cast<DWORD>((fixed.size() + 1) * sizeof(wchar_t)));
            ::RegCloseKey(key);
        }
    }
}

}  // namespace snippin
