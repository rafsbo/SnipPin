#include "snippet/RegionSelector.h"

#include <cstdlib>

#include "app/Strings.h"
#include "platform/ApiShim.h"
#include "platform/WinUtil.h"

namespace snippin {
namespace {

constexpr wchar_t kClassName[] = L"SnipPin.RegionSelector";
constexpr COLORREF kColorKey = RGB(255, 0, 255);  // fully transparent "hole"
constexpr COLORREF kDim = RGB(0, 0, 0);
constexpr COLORREF kAccent = RGB(0, 120, 215);
constexpr BYTE kOverlayAlpha = 110;

void RegisterClassOnce(HINSTANCE instance, WNDPROC proc) {
    static bool registered = false;
    if (registered) {
        return;
    }
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = proc;
    wc.hInstance = instance;
    wc.hCursor = ::LoadCursorW(nullptr, IDC_CROSS);
    wc.lpszClassName = kClassName;
    registered = ::RegisterClassExW(&wc) != 0;
}

}  // namespace

LRESULT CALLBACK RegionSelector::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    RegionSelector* self = nullptr;
    if (msg == WM_NCCREATE) {
        self = static_cast<RegionSelector*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<RegionSelector*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    // After Finish()/Cancel() detach the object, remaining destruction
    // messages go straight to the default procedure.
    if (!self || self->hwnd_ != hwnd) {
        if (msg == WM_NCDESTROY) {
            ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        }
        return ::DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return self->HandleMessage(msg, wParam, lParam);
}

RegionSelector::~RegionSelector() { Cancel(); }

bool RegionSelector::Begin(HINSTANCE instance, HWND target, Callback onSelected) {
    Cancel();
    if (!target || !::IsWindow(target) || ::IsIconic(target)) {
        return false;
    }
    RegisterClassOnce(instance, &RegionSelector::WndProc);
    target_ = target;
    onSelected_ = std::move(onSelected);
    overlayScreen_ = win::GetVisibleFrame(target);
    ::GetWindowRect(target, &targetWindow_);
    dragging_ = false;
    finishing_ = false;

    // Make sure the user sees the window they are snipping.
    win::ForceForeground(target);

    const int w = overlayScreen_.right - overlayScreen_.left;
    const int h = overlayScreen_.bottom - overlayScreen_.top;
    ::CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED, kClassName, L"SnipPin region selector",
                      WS_POPUP, overlayScreen_.left, overlayScreen_.top, w, h, nullptr, nullptr, instance, this);
    if (!hwnd_) {
        return false;
    }
    ::SetLayeredWindowAttributes(hwnd_, kColorKey, kOverlayAlpha, LWA_ALPHA | LWA_COLORKEY);
    ::ShowWindow(hwnd_, SW_SHOW);
    win::ForceForeground(hwnd_);
    ::SetFocus(hwnd_);
    return true;
}

void RegionSelector::Cancel() {
    if (hwnd_) {
        finishing_ = true;
        HWND hwnd = hwnd_;
        hwnd_ = nullptr;
        ::DestroyWindow(hwnd);
    }
    onSelected_ = nullptr;
}

logic::Rect RegionSelector::SelectionInClient() const {
    return logic::NormalizeRect(anchor_.x, anchor_.y, current_.x, current_.y);
}

void RegionSelector::Finish(bool accepted) {
    if (finishing_) {
        return;
    }
    finishing_ = true;
    if (::GetCapture() == hwnd_) {
        ::ReleaseCapture();
    }
    const logic::Rect client{0, 0, overlayScreen_.right - overlayScreen_.left,
                             overlayScreen_.bottom - overlayScreen_.top};
    logic::Rect selection = dragging_ ? logic::ClampRect(SelectionInClient(), client) : client;
    const int minSize = ::GetSystemMetrics(SM_CXDRAG) * 2;
    if (selection.Width() < minSize || selection.Height() < minSize) {
        selection = client;  // tiny drag behaves like a click: whole window
    }

    Callback callback = std::move(onSelected_);
    HWND target = target_;
    HWND hwnd = hwnd_;
    hwnd_ = nullptr;
    ::DestroyWindow(hwnd);

    if (accepted && callback && ::IsWindow(target)) {
        const logic::Rect screen = logic::OffsetRect(selection, overlayScreen_.left, overlayScreen_.top);
        const logic::Rect inWindow = logic::OffsetRect(screen, -targetWindow_.left, -targetWindow_.top);
        callback(target, inWindow, screen);
    }
}

void RegionSelector::Paint(HDC dc) {
    RECT client;
    ::GetClientRect(hwnd_, &client);
    HDC mem = ::CreateCompatibleDC(dc);
    GdiObject<HBITMAP> bitmap(::CreateCompatibleBitmap(dc, client.right, client.bottom));
    HGDIOBJ oldBitmap = ::SelectObject(mem, bitmap.Get());

    GdiObject<HBRUSH> dim(::CreateSolidBrush(kDim));
    ::FillRect(mem, &client, dim.Get());

    const UINT dpi = api::GetDpiForWindow(hwnd_);
    if (dragging_) {
        RECT sel = ToRECT(SelectionInClient());
        GdiObject<HBRUSH> hole(::CreateSolidBrush(kColorKey));
        ::FillRect(mem, &sel, hole.Get());
        GdiObject<HBRUSH> accent(::CreateSolidBrush(kAccent));
        const int t = api::Scale(2, dpi);
        RECT edge = sel;
        ::InflateRect(&edge, t, t);
        for (int i = 0; i < t; ++i) {
            ::FrameRect(mem, &edge, accent.Get());
            ::InflateRect(&edge, -1, -1);
        }
        wchar_t size[48];
        _snwprintf_s(size, _countof(size), _TRUNCATE, L"%d \u00D7 %d", sel.right - sel.left, sel.bottom - sel.top);
        ::SetBkMode(mem, TRANSPARENT);
        ::SetTextColor(mem, RGB(255, 255, 255));
        RECT label{sel.left, sel.bottom + t + 2, sel.left + api::Scale(160, dpi), sel.bottom + api::Scale(24, dpi)};
        ::DrawTextW(mem, size, -1, &label, DT_SINGLELINE | DT_LEFT | DT_TOP | DT_NOCLIP);
    }

    // Hint banner.
    NONCLIENTMETRICSW metrics;
    GdiObject<HFONT> font;
    if (api::GetNonClientMetricsForDpi(metrics, dpi)) {
        font.Reset(::CreateFontIndirectW(&metrics.lfMessageFont));
    }
    HGDIOBJ oldFont = font ? ::SelectObject(mem, font.Get()) : nullptr;
    ::SetBkMode(mem, TRANSPARENT);
    ::SetTextColor(mem, RGB(255, 255, 255));
    RECT banner{0, api::Scale(8, dpi), client.right, api::Scale(40, dpi)};
    ::DrawTextW(mem, str::kRegionSelectorHint, -1, &banner, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    if (oldFont) {
        ::SelectObject(mem, oldFont);
    }

    ::BitBlt(dc, 0, 0, client.right, client.bottom, mem, 0, 0, SRCCOPY);
    ::SelectObject(mem, oldBitmap);
    ::DeleteDC(mem);
}

LRESULT RegionSelector::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = ::BeginPaint(hwnd_, &ps);
            Paint(dc);
            ::EndPaint(hwnd_, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN:
            anchor_ = current_ = POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            dragging_ = false;
            ::SetCapture(hwnd_);
            return 0;
        case WM_MOUSEMOVE:
            if (::GetCapture() == hwnd_) {
                current_ = POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                if (!dragging_ && (std::abs(current_.x - anchor_.x) > ::GetSystemMetrics(SM_CXDRAG) ||
                                   std::abs(current_.y - anchor_.y) > ::GetSystemMetrics(SM_CYDRAG))) {
                    dragging_ = true;
                }
                ::InvalidateRect(hwnd_, nullptr, FALSE);
            }
            return 0;
        case WM_LBUTTONUP:
            if (::GetCapture() == hwnd_) {
                current_ = POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                Finish(true);
            }
            return 0;
        case WM_RBUTTONUP:
            Finish(false);
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                Finish(false);
            } else if (wParam == VK_RETURN) {
                Finish(true);
            }
            return 0;
        case WM_CAPTURECHANGED:
        case WM_ACTIVATE:
            if (msg == WM_ACTIVATE && LOWORD(wParam) != WA_INACTIVE) {
                break;
            }
            if (msg == WM_CAPTURECHANGED && reinterpret_cast<HWND>(lParam) == hwnd_) {
                break;
            }
            if (!finishing_ && (msg == WM_ACTIVATE || dragging_)) {
                Finish(false);  // user switched away (Alt+Tab, Win key, ...)
            }
            return 0;
        default:
            break;
    }
    return ::DefWindowProcW(hwnd_, msg, wParam, lParam);
}

}  // namespace snippin
