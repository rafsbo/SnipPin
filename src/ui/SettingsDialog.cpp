#include "ui/SettingsDialog.h"

#include <algorithm>
#include <iterator>

#include "i18n/Catalog.h"
#include "resource.h"

namespace snippin {
namespace {

using i18n::StringId;
using i18n::Tr;

constexpr unsigned kModifierChoices[] = {
    logic::kModControl,
    logic::kModAlt,
    logic::kModShift,
    logic::kModControl | logic::kModShift,
    logic::kModControl | logic::kModAlt,
    logic::kModAlt | logic::kModShift,
};

// Control id -> translated caption.
const struct {
    int id;
    StringId text;
} kCaptions[] = {
    {IDC_GROUP_HOTKEYS, StringId::SettingsGroupHotkeys},
    {IDC_LABEL_HOTKEY_FIRST + 0, StringId::SettingsHkTopmost},
    {IDC_LABEL_HOTKEY_FIRST + 1, StringId::SettingsHkPick},
    {IDC_LABEL_HOTKEY_FIRST + 2, StringId::SettingsHkOpacityDown},
    {IDC_LABEL_HOTKEY_FIRST + 3, StringId::SettingsHkOpacityUp},
    {IDC_LABEL_HOTKEY_FIRST + 4, StringId::SettingsHkGhost},
    {IDC_LABEL_HOTKEY_FIRST + 5, StringId::SettingsHkSnip},
    {IDC_LABEL_HOTKEY_FIRST + 6, StringId::SettingsHkStash},
    {IDC_LABEL_HOTKEY_FIRST + 7, StringId::SettingsHkPanic},
    {IDC_GROUP_GESTURE, StringId::SettingsGroupGesture},
    {IDC_CTXMENU_ENABLED, StringId::SettingsGestureEnabled},
    {IDC_GROUP_SNIPPETS, StringId::SettingsGroupSnippets},
    {IDC_SNIPPET_LIVE, StringId::SettingsSnippetLive},
    {IDC_SNIPPET_COMPAT, StringId::SettingsSnippetCompat},
    {IDC_GROUP_GENERAL, StringId::SettingsGroupGeneral},
    {IDC_LABEL_LANGUAGE, StringId::SettingsLanguage},
    {IDC_RUN_AT_STARTUP, StringId::SettingsRunAtStartup},
    {IDC_ZORDER_GUARD, StringId::SettingsZOrderGuard},
    {IDC_NOTIFICATIONS, StringId::SettingsNotifications},
    {IDC_LABEL_OPACITY_STEP, StringId::SettingsOpacityStep},
    {IDC_LABEL_GHOST_OPACITY, StringId::SettingsGhostOpacity},
    {IDC_LABEL_HISTORY_SIZE, StringId::SettingsHistorySize},
    {IDC_DEFAULTS, StringId::SettingsDefaults},
    {IDOK, StringId::SettingsOk},
    {IDCANCEL, StringId::SettingsCancel},
};

void ApplyCaptions(HWND dlg) {
    ::SetWindowTextW(dlg, Tr(StringId::SettingsTitle));
    for (const auto& c : kCaptions) {
        ::SetDlgItemTextW(dlg, c.id, Tr(c.text));
    }
}

// Language combo: item 0 = automatic, then one item per language. The item
// data is the index into SettingsDialogResult::languages (+1), 0 = auto.
void PopulateLanguages(HWND dlg, const SettingsDialogResult& data) {
    HWND combo = ::GetDlgItem(dlg, IDC_LANGUAGE);
    ::SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    ::SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Tr(StringId::SettingsLanguageAuto)));
    ::SendMessageW(combo, CB_SETITEMDATA, 0, 0);
    int selected = 0;
    for (size_t i = 0; i < data.languages.size(); ++i) {
        const auto& lang = data.languages[i];
        std::wstring label = lang.nativeName;
        if (!lang.englishName.empty() && lang.englishName != lang.nativeName) {
            label += L" (" + lang.englishName + L")";
        }
        const auto item = ::SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        ::SendMessageW(combo, CB_SETITEMDATA, static_cast<WPARAM>(item), static_cast<LPARAM>(i + 1));
        if (lang.code == data.settings.language) {
            selected = static_cast<int>(item);
        }
    }
    ::SendMessageW(combo, CB_SETCURSEL, static_cast<WPARAM>(selected), 0);
}

bool IsExtendedKey(unsigned vk) {
    switch (vk) {
        case VK_UP: case VK_DOWN: case VK_LEFT: case VK_RIGHT:
        case VK_INSERT: case VK_DELETE: case VK_HOME: case VK_END:
        case VK_PRIOR: case VK_NEXT: case VK_DIVIDE:
            return true;
        default:
            return false;
    }
}

WORD ToHotkeyControl(const logic::Hotkey& hk) {
    BYTE flags = 0;
    if (hk.modifiers & logic::kModShift) flags |= HOTKEYF_SHIFT;
    if (hk.modifiers & logic::kModControl) flags |= HOTKEYF_CONTROL;
    if (hk.modifiers & logic::kModAlt) flags |= HOTKEYF_ALT;
    if (IsExtendedKey(hk.vk)) flags |= HOTKEYF_EXT;
    return MAKEWORD(static_cast<BYTE>(hk.vk), flags);
}

logic::Hotkey FromHotkeyControl(WORD value, const logic::Hotkey& previous) {
    logic::Hotkey hk;
    hk.vk = LOBYTE(value);
    const BYTE flags = HIBYTE(value);
    if (flags & HOTKEYF_SHIFT) hk.modifiers |= logic::kModShift;
    if (flags & HOTKEYF_CONTROL) hk.modifiers |= logic::kModControl;
    if (flags & HOTKEYF_ALT) hk.modifiers |= logic::kModAlt;
    // The common control cannot show the Win key: keep it if unchanged.
    if ((previous.modifiers & logic::kModWin) && hk.vk == previous.vk &&
        (hk.modifiers | logic::kModWin) == previous.modifiers) {
        hk.modifiers = previous.modifiers;
    }
    return hk;
}

void Populate(HWND dlg, const SettingsDialogResult& data) {
    const AppSettings& s = data.settings;
    for (int i = 0; i < static_cast<int>(HotkeyAction::Count); ++i) {
        HWND control = ::GetDlgItem(dlg, IDC_HOTKEY_FIRST + i);
        // Refuse shortcuts without modifiers: they would hijack normal typing.
        ::SendMessageW(control, HKM_SETRULES, HKCOMB_NONE | HKCOMB_S, MAKELPARAM(HOTKEYF_CONTROL | HOTKEYF_ALT, 0));
        ::SendMessageW(control, HKM_SETHOTKEY, ToHotkeyControl(s.hotkeys[i]), 0);
    }

    ::CheckDlgButton(dlg, IDC_CTXMENU_ENABLED, s.contextMenuEnabled ? BST_CHECKED : BST_UNCHECKED);
    HWND combo = ::GetDlgItem(dlg, IDC_CTXMENU_MODIFIERS);
    ::SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    int selected = 0;
    for (int i = 0; i < static_cast<int>(std::size(kModifierChoices)); ++i) {
        const std::wstring label = i18n::TrFormat(StringId::SettingsGestureFormat,
                                                  {logic::FormatModifiers(kModifierChoices[i])});
        ::SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        if (kModifierChoices[i] == s.contextMenuModifiers) {
            selected = i;
        }
    }
    ::SendMessageW(combo, CB_SETCURSEL, static_cast<WPARAM>(selected), 0);

    ::CheckDlgButton(dlg, IDC_RUN_AT_STARTUP, data.runAtStartup ? BST_CHECKED : BST_UNCHECKED);
    ::CheckDlgButton(dlg, IDC_ZORDER_GUARD, s.zOrderGuardEnabled ? BST_CHECKED : BST_UNCHECKED);
    ::CheckDlgButton(dlg, IDC_NOTIFICATIONS, s.showNotifications ? BST_CHECKED : BST_UNCHECKED);
    ::CheckDlgButton(dlg, IDC_SNIPPET_LIVE, s.snippetLiveByDefault ? BST_CHECKED : BST_UNCHECKED);
    ::CheckDlgButton(dlg, IDC_SNIPPET_COMPAT, s.snippetCompatibilityMode ? BST_CHECKED : BST_UNCHECKED);
    ::SetDlgItemInt(dlg, IDC_OPACITY_STEP, static_cast<UINT>(s.opacityStepPercent), FALSE);
    ::SetDlgItemInt(dlg, IDC_GHOST_OPACITY, static_cast<UINT>(s.ghostOpacityPercent), FALSE);
    ::SetDlgItemInt(dlg, IDC_HISTORY_SIZE, static_cast<UINT>(s.historySize), FALSE);
    PopulateLanguages(dlg, data);
}

int ReadInt(HWND dlg, int id, int fallback, int lo, int hi) {
    BOOL ok = FALSE;
    const UINT value = ::GetDlgItemInt(dlg, id, &ok, FALSE);
    return ok ? std::clamp(static_cast<int>(value), lo, hi) : fallback;
}

void Collect(HWND dlg, SettingsDialogResult& data) {
    AppSettings& s = data.settings;
    for (int i = 0; i < static_cast<int>(HotkeyAction::Count); ++i) {
        const auto value = static_cast<WORD>(::SendDlgItemMessageW(dlg, IDC_HOTKEY_FIRST + i, HKM_GETHOTKEY, 0, 0));
        s.hotkeys[i] = FromHotkeyControl(value, s.hotkeys[i]);
    }
    s.contextMenuEnabled = ::IsDlgButtonChecked(dlg, IDC_CTXMENU_ENABLED) == BST_CHECKED;
    const auto sel = ::SendDlgItemMessageW(dlg, IDC_CTXMENU_MODIFIERS, CB_GETCURSEL, 0, 0);
    if (sel >= 0 && sel < static_cast<LRESULT>(std::size(kModifierChoices))) {
        s.contextMenuModifiers = kModifierChoices[sel];
    }
    const auto langItem = ::SendDlgItemMessageW(dlg, IDC_LANGUAGE, CB_GETCURSEL, 0, 0);
    if (langItem >= 0) {
        const auto index = static_cast<size_t>(::SendDlgItemMessageW(dlg, IDC_LANGUAGE, CB_GETITEMDATA,
                                                                     static_cast<WPARAM>(langItem), 0));
        s.language = (index == 0 || index > data.languages.size()) ? std::wstring(L"auto")
                                                                    : data.languages[index - 1].code;
    }
    data.runAtStartup = ::IsDlgButtonChecked(dlg, IDC_RUN_AT_STARTUP) == BST_CHECKED;
    s.zOrderGuardEnabled = ::IsDlgButtonChecked(dlg, IDC_ZORDER_GUARD) == BST_CHECKED;
    s.showNotifications = ::IsDlgButtonChecked(dlg, IDC_NOTIFICATIONS) == BST_CHECKED;
    s.snippetLiveByDefault = ::IsDlgButtonChecked(dlg, IDC_SNIPPET_LIVE) == BST_CHECKED;
    s.snippetCompatibilityMode = ::IsDlgButtonChecked(dlg, IDC_SNIPPET_COMPAT) == BST_CHECKED;
    s.opacityStepPercent = ReadInt(dlg, IDC_OPACITY_STEP, s.opacityStepPercent, 1, 50);
    s.ghostOpacityPercent = ReadInt(dlg, IDC_GHOST_OPACITY, s.ghostOpacityPercent, s.minOpacityPercent, 100);
    s.historySize = ReadInt(dlg, IDC_HISTORY_SIZE, s.historySize, 0, 100);
}

INT_PTR CALLBACK DialogProc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* data = reinterpret_cast<SettingsDialogResult*>(::GetWindowLongPtrW(dlg, DWLP_USER));
    switch (msg) {
        case WM_INITDIALOG:
            ::SetWindowLongPtrW(dlg, DWLP_USER, lParam);
            data = reinterpret_cast<SettingsDialogResult*>(lParam);
            ApplyCaptions(dlg);
            Populate(dlg, *data);
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDOK:
                    Collect(dlg, *data);
                    ::EndDialog(dlg, IDOK);
                    return TRUE;
                case IDCANCEL:
                    ::EndDialog(dlg, IDCANCEL);
                    return TRUE;
                case IDC_DEFAULTS: {
                    SettingsDialogResult defaults;
                    defaults.settings = AppSettings::Defaults();
                    defaults.settings.firstRunDone = data->settings.firstRunDone;
                    defaults.runAtStartup = false;
                    defaults.languages = data->languages;
                    Populate(dlg, defaults);
                    return TRUE;
                }
                default:
                    break;
            }
            break;
        default:
            break;
    }
    return FALSE;
}

}  // namespace

bool ShowSettingsDialog(HINSTANCE instance, HWND owner, SettingsDialogResult& inOut) {
    SettingsDialogResult working = inOut;
    const INT_PTR result = ::DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_SETTINGS), owner, &DialogProc,
                                             reinterpret_cast<LPARAM>(&working));
    if (result != IDOK) {
        return false;
    }
    inOut = working;
    return true;
}

}  // namespace snippin
