#include "ui/SettingsDialog.h"

#include <algorithm>
#include <iterator>

#include "resource.h"

namespace snippin {
namespace {

struct ModifierChoice {
    unsigned modifiers;
    const wchar_t* label;
};

constexpr ModifierChoice kModifierChoices[] = {
    {logic::kModControl, L"Ctrl + right-click"},
    {logic::kModAlt, L"Alt + right-click"},
    {logic::kModShift, L"Shift + right-click"},
    {logic::kModControl | logic::kModShift, L"Ctrl + Shift + right-click"},
    {logic::kModControl | logic::kModAlt, L"Ctrl + Alt + right-click"},
    {logic::kModAlt | logic::kModShift, L"Alt + Shift + right-click"},
};

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
        ::SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(kModifierChoices[i].label));
        if (kModifierChoices[i].modifiers == s.contextMenuModifiers) {
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
        s.contextMenuModifiers = kModifierChoices[sel].modifiers;
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
