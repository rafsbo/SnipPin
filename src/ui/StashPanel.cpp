#include "ui/StashPanel.h"

#include "app/AppInfo.h"
#include "i18n/Catalog.h"
#include "platform/ApiShim.h"
#include "platform/WinUtil.h"

namespace snippin {

using i18n::StringId;
using i18n::Tr;
using i18n::TrFormat;
namespace {

constexpr wchar_t kClassName[] = L"SnipPin.StashPanel";
constexpr int kListId = 100;

}  // namespace

StashPanel::~StashPanel() { Destroy(); }

bool StashPanel::Create(HINSTANCE instance, RowProvider rows, ItemHandler onActivate, MenuHandler onContextMenu) {
    instance_ = instance;
    rows_ = std::move(rows);
    onActivate_ = std::move(onActivate);
    onContextMenu_ = std::move(onContextMenu);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &StashPanel::WndProc;
    wc.hInstance = instance;
    wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.hIcon = ::LoadIconW(instance, MAKEINTRESOURCEW(1));
    wc.lpszClassName = kClassName;
    ::RegisterClassExW(&wc);

    hwnd_ = ::CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, kClassName, Tr(StringId::StashTitle),
                              WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_CLIPCHILDREN,
                              CW_USEDEFAULT, CW_USEDEFAULT, 360, 480, nullptr, nullptr, instance, this);
    if (!hwnd_) {
        return false;
    }
    CreateList();
    ApplyDpi(api::GetDpiForWindow(hwnd_));
    return true;
}

void StashPanel::Destroy() {
    if (hwnd_) {
        HWND h = hwnd_;
        hwnd_ = nullptr;
        ::DestroyWindow(h);
    }
    if (images_) {
        ImageList_Destroy(images_);
        images_ = nullptr;
    }
}

void StashPanel::CreateList() {
    list_ = ::CreateWindowExW(0, WC_LISTVIEWW, L"",
                              WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS |
                                  LVS_NOSORTHEADER,
                              0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kListId)), instance_,
                              nullptr);
    ListView_SetExtendedListViewStyle(list_, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP);
    // Explorer visual style on Vista+ (harmless if unavailable).
    using SetWindowThemeFn = HRESULT(WINAPI*)(HWND, LPCWSTR, LPCWSTR);
    if (HMODULE uxtheme = ::LoadLibraryW(L"uxtheme.dll")) {
        if (auto setTheme = reinterpret_cast<SetWindowThemeFn>(
                reinterpret_cast<void*>(::GetProcAddress(uxtheme, "SetWindowTheme")))) {
            setTheme(list_, L"Explorer", nullptr);
        }
    }

    LVCOLUMNW col{};
    col.mask = LVCF_WIDTH;
    col.cx = 220;
    ListView_InsertColumn(list_, 0, &col);
    col.cx = 100;
    ListView_InsertColumn(list_, 1, &col);

    ListView_EnableGroupView(list_, TRUE);
    for (PanelGroup id : {PanelGroup::Pinned, PanelGroup::Snippets, PanelGroup::Recent}) {
        LVGROUP group{};
        group.cbSize = sizeof(group);
        group.mask = LVGF_GROUPID | LVGF_HEADER;
        group.pszHeader = const_cast<wchar_t*>(L"");  // caption set by ApplyLanguage()
        group.iGroupId = static_cast<int>(id);
        ListView_InsertGroup(list_, -1, &group);
    }
    ApplyLanguage();
}

void StashPanel::ApplyLanguage() {
    if (!hwnd_ || !list_) {
        return;
    }
    ::SetWindowTextW(hwnd_, Tr(StringId::StashTitle));
    const StringId columns[] = {StringId::StashColumnWindow, StringId::StashColumnState};
    for (int i = 0; i < 2; ++i) {
        LVCOLUMNW col{};
        col.mask = LVCF_TEXT;
        col.pszText = const_cast<wchar_t*>(Tr(columns[i]));
        ListView_SetColumn(list_, i, &col);
    }
    const struct {
        PanelGroup id;
        StringId name;
    } groups[] = {{PanelGroup::Pinned, StringId::StashGroupPinned},
                  {PanelGroup::Snippets, StringId::StashGroupSnippets},
                  {PanelGroup::Recent, StringId::StashGroupRecent}};
    for (const auto& g : groups) {
        LVGROUP group{};
        group.cbSize = sizeof(group);
        group.mask = LVGF_HEADER;
        group.pszHeader = const_cast<wchar_t*>(Tr(g.name));
        ListView_SetGroupInfo(list_, static_cast<int>(g.id), &group);
    }
    Refresh();  // state texts ("On top", "Ghost", ...) are translated too
}

void StashPanel::ApplyDpi(UINT dpi) {
    dpi_ = dpi ? dpi : 96;
    NONCLIENTMETRICSW metrics;
    if (api::GetNonClientMetricsForDpi(metrics, dpi_)) {
        font_.Reset(::CreateFontIndirectW(&metrics.lfMessageFont));
        ::SendMessageW(list_, WM_SETFONT, reinterpret_cast<WPARAM>(font_.Get()), TRUE);
    }
    ListView_SetColumnWidth(list_, 0, api::Scale(220, dpi_));
    ListView_SetColumnWidth(list_, 1, api::Scale(100, dpi_));
    Refresh();
}

void StashPanel::DockToCursorMonitor() {
    POINT cursor;
    ::GetCursorPos(&cursor);
    const RECT work = win::GetWorkAreaFromPoint(cursor);
    const UINT dpi = api::GetDpiForWindow(hwnd_);
    const int width = api::Scale(360, dpi);
    const int height = (work.bottom - work.top) * 3 / 5;
    const int margin = api::Scale(8, dpi);
    ::SetWindowPos(hwnd_, HWND_TOPMOST, work.right - width - margin, work.top + margin, width, height,
                   SWP_NOACTIVATE);
}

void StashPanel::LayoutList() {
    RECT rc;
    ::GetClientRect(hwnd_, &rc);
    ::MoveWindow(list_, 0, 0, rc.right, rc.bottom, TRUE);
}

void StashPanel::Toggle() {
    if (IsVisible()) {
        Hide();
    } else {
        Show();
    }
}

void StashPanel::Show() {
    if (!hwnd_) {
        return;
    }
    if (!IsVisible()) {
        DockToCursorMonitor();
    }
    ::ShowWindow(hwnd_, SW_SHOW);
    Refresh();
    win::ForceForeground(hwnd_);
    ::SetFocus(list_);
}

void StashPanel::Hide() {
    if (hwnd_) {
        ::ShowWindow(hwnd_, SW_HIDE);
    }
}

bool StashPanel::IsVisible() const { return hwnd_ && ::IsWindowVisible(hwnd_); }

const PanelItemRef* StashPanel::RefAt(int index) const {
    if (index < 0) {
        return nullptr;
    }
    LVITEMW item{};
    item.mask = LVIF_PARAM;
    item.iItem = index;
    if (!ListView_GetItem(list_, &item)) {
        return nullptr;
    }
    const auto slot = static_cast<size_t>(item.lParam);
    return slot < refs_.size() ? &refs_[slot] : nullptr;
}

void StashPanel::Refresh() {
    if (!hwnd_ || !list_ || !rows_ || !IsVisible()) {
        return;
    }
    const std::vector<PanelRow> rows = rows_();
    ::SendMessageW(list_, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(list_);
    refs_.clear();

    const int iconSize = api::Scale(16, dpi_);
    if (images_) {
        ImageList_Destroy(images_);
    }
    images_ = ImageList_Create(iconSize, iconSize, ILC_COLOR32 | ILC_MASK, static_cast<int>(rows.size()) + 1, 4);
    const int fallbackIcon = ImageList_AddIcon(images_, ::LoadIconW(nullptr, IDI_APPLICATION));
    ListView_SetImageList(list_, images_, LVSIL_SMALL);

    int index = 0;
    for (const auto& row : rows) {
        int image = fallbackIcon;
        if (row.icon) {
            image = ImageList_AddIcon(images_, row.icon);
            if (row.ownsIcon) {
                ::DestroyIcon(row.icon);
            }
            if (image < 0) {
                image = fallbackIcon;
            }
        }
        refs_.push_back(row.ref);

        LVITEMW item{};
        item.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE | LVIF_GROUPID;
        item.iItem = index;
        item.pszText = const_cast<wchar_t*>(row.title.c_str());
        item.lParam = static_cast<LPARAM>(refs_.size() - 1);
        item.iImage = image;
        item.iGroupId = static_cast<int>(row.ref.group);
        const int inserted = ListView_InsertItem(list_, &item);
        if (inserted >= 0) {
            ListView_SetItemText(list_, inserted, 1, const_cast<wchar_t*>(row.state.c_str()));
        }
        ++index;
    }
    ::SendMessageW(list_, WM_SETREDRAW, TRUE, 0);
    ::InvalidateRect(list_, nullptr, TRUE);
}

LRESULT CALLBACK StashPanel::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    StashPanel* self = nullptr;
    if (msg == WM_NCCREATE) {
        self = static_cast<StashPanel*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<StashPanel*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (!self || self->hwnd_ != hwnd) {
        return ::DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return self->HandleMessage(msg, wParam, lParam);
}

LRESULT StashPanel::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SIZE:
            LayoutList();
            return 0;
        case WM_CLOSE:
            Hide();
            return 0;
        case WM_ACTIVATE:
            if (LOWORD(wParam) != WA_INACTIVE) {
                Refresh();
            }
            break;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                Hide();
                return 0;
            }
            break;
        case kWmDpiChanged: {
            const auto* suggested = reinterpret_cast<const RECT*>(lParam);
            ::SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                           suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
            ApplyDpi(HIWORD(wParam));
            return 0;
        }
        case WM_NOTIFY: {
            const auto* header = reinterpret_cast<const NMHDR*>(lParam);
            if (header->idFrom != kListId) {
                break;
            }
            if (header->code == NM_DBLCLK || header->code == LVN_ITEMACTIVATE) {
                const auto* activate = reinterpret_cast<const NMITEMACTIVATE*>(lParam);
                if (const PanelItemRef* ref = RefAt(activate->iItem)) {
                    const PanelItemRef copy = *ref;
                    if (onActivate_) {
                        onActivate_(copy);
                    }
                }
                return 0;
            }
            if (header->code == NM_RCLICK) {
                const auto* click = reinterpret_cast<const NMITEMACTIVATE*>(lParam);
                if (const PanelItemRef* ref = RefAt(click->iItem)) {
                    const PanelItemRef copy = *ref;
                    POINT pt;
                    ::GetCursorPos(&pt);
                    if (onContextMenu_) {
                        onContextMenu_(copy, pt);
                    }
                }
                return 0;
            }
            if (header->code == LVN_KEYDOWN) {
                const auto* key = reinterpret_cast<const NMLVKEYDOWN*>(lParam);
                if (key->wVKey == VK_ESCAPE) {
                    Hide();
                }
                if (key->wVKey == VK_APPS || (key->wVKey == VK_F10 && (::GetKeyState(VK_SHIFT) & 0x8000))) {
                    const int selected = ListView_GetNextItem(list_, -1, LVNI_SELECTED);
                    if (const PanelItemRef* ref = RefAt(selected)) {
                        const PanelItemRef copy = *ref;
                        RECT itemRect{};
                        ListView_GetItemRect(list_, selected, &itemRect, LVIR_LABEL);
                        POINT pt{itemRect.left, itemRect.bottom};
                        ::ClientToScreen(list_, &pt);
                        if (onContextMenu_) {
                            onContextMenu_(copy, pt);
                        }
                    }
                }
            }
            break;
        }
        default:
            break;
    }
    return ::DefWindowProcW(hwnd_, msg, wParam, lParam);
}

}  // namespace snippin
