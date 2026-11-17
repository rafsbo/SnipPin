#include "ui/WindowPicker.h"

#include "app/Strings.h"
#include "platform/ApiShim.h"
#include "platform/WinUtil.h"

namespace snippin {
namespace {

constexpr wchar_t kOverlayClass[] = L"SnipPin.PickerOverlay";
constexpr wchar_t kFrameClass[] = L"SnipPin.PickerFrame";
constexpr COLORREF kColorKey = RGB(255, 0, 255);
constexpr COLORREF kAccent = RGB(0, 120, 215);
constexpr LONG_PTR kModeFrame = 1;
constexpr LONG_PTR kModeHint = 2;

void RegisterClasses(HINSTANCE instance, WNDPROC overlayProc, WNDPROC frameProc) {
    static bool registered = false;
    if (registered) {
        return;
    }
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = overlayProc;
    wc.hInstance = instance;
    wc.hCursor = ::LoadCursorW(nullptr, IDC_CROSS);
    wc.lpszClassName = kOverlayClass;
    ::RegisterClassExW(&wc);

    wc.lpfnWndProc = frameProc;
    wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kFrameClass;
    ::RegisterClassExW(&wc);
    registered = true;
}

}  // namespace

WindowPicker::~WindowPicker() { Cancel(); }

bool WindowPicker::Begin(HINSTANCE instance, Callback onPicked) {
    Cancel();
    RegisterClasses(instance, &WindowPicker::OverlayProc, &WindowPicker::FrameProc);
    onPicked_ = std::move(onPicked);

    const int x = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int y = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int w = ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int h = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);

    // Outline window (click-through, colour-keyed interior).
    frame_ = ::CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
                               kFrameClass, L"", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, instance, nullptr);
    if (frame_) {
        ::SetWindowLongPtrW(frame_, GWLP_USERDATA, kModeFrame);
        ::SetLayeredWindowAttributes(frame_, kColorKey, 255, LWA_COLORKEY);
    }

    // Overlay: alpha 1 is invisible but still receives mouse input.
    overlay_ = ::CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED, kOverlayClass,
                                 L"SnipPin window picker", WS_POPUP, x, y, w, h, nullptr, nullptr, instance, this);
    if (!overlay_) {
        Cancel();
        return false;
    }
    ::SetLayeredWindowAttributes(overlay_, 0, 1, LWA_ALPHA);

    // Hint banner at the top of the monitor under the cursor.
    POINT cursor;
    ::GetCursorPos(&cursor);
    const RECT work = win::GetWorkAreaFromPoint(cursor);
    const UINT dpi = api::GetSystemDpi();
    const int hintW = api::Scale(420, dpi);
    const int hintH = api::Scale(32, dpi);
    hint_ = ::CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
                              kFrameClass, L"", WS_POPUP, work.left + (work.right - work.left - hintW) / 2,
                              work.top + api::Scale(16, dpi), hintW, hintH, nullptr, nullptr, instance, nullptr);
    if (hint_) {
        ::SetWindowLongPtrW(hint_, GWLP_USERDATA, kModeHint);
        ::SetLayeredWindowAttributes(hint_, 0, 230, LWA_ALPHA);
        ::ShowWindow(hint_, SW_SHOWNOACTIVATE);
    }

    ::ShowWindow(overlay_, SW_SHOW);
    win::ForceForeground(overlay_);
    ::SetFocus(overlay_);
    ::SetCapture(overlay_);
    UpdateHighlight(cursor);
    return true;
}

void WindowPicker::Cancel() {
    for (HWND* w : {&overlay_, &frame_, &hint_}) {
        if (*w) {
            HWND h = *w;
            *w = nullptr;
            ::DestroyWindow(h);
        }
    }
    current_ = nullptr;
    onPicked_ = nullptr;
}

namespace {

struct WindowAtContext {
    POINT pt;
    HWND found;
};

// Explicit CALLBACK function: lambdas do not convert to __stdcall on x86 GCC.
BOOL CALLBACK WindowAtProc(HWND hwnd, LPARAM lp) {
    auto* c = reinterpret_cast<WindowAtContext*>(lp);
    if (!win::IsCandidateWindow(hwnd) || ::IsIconic(hwnd)) {
        return TRUE;
    }
    const RECT rc = win::GetVisibleFrame(hwnd);
    if (::PtInRect(&rc, c->pt)) {
        c->found = hwnd;
        return FALSE;
    }
    return TRUE;
}

}  // namespace

HWND WindowPicker::WindowAt(POINT pt) {
    WindowAtContext ctx{pt, nullptr};
    // EnumWindows walks top-level windows in z-order, topmost first.
    ::EnumWindows(&WindowAtProc, reinterpret_cast<LPARAM>(&ctx));
    return ctx.found;
}

void WindowPicker::UpdateHighlight(POINT pt) {
    HWND target = WindowAt(pt);
    if (target == current_) {
        return;
    }
    current_ = target;
    if (!frame_) {
        return;
    }
    if (!target) {
        ::ShowWindow(frame_, SW_HIDE);
        return;
    }
    const RECT rc = win::GetVisibleFrame(target);
    ::SetWindowPos(frame_, HWND_TOPMOST, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
                   SWP_NOACTIVATE | SWP_SHOWWINDOW);
    ::InvalidateRect(frame_, nullptr, TRUE);
    // Keep the overlay above the outline so it still gets the clicks.
    ::SetWindowPos(overlay_, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void WindowPicker::Finish(bool accepted, POINT pt) {
    Callback callback = std::move(onPicked_);
    HWND picked = current_;
    if (::GetCapture() == overlay_) {
        ::ReleaseCapture();
    }
    Cancel();
    if (accepted && picked && callback) {
        callback(picked, pt);
    }
}

LRESULT CALLBACK WindowPicker::OverlayProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WindowPicker* self = nullptr;
    if (msg == WM_NCCREATE) {
        self = static_cast<WindowPicker*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<WindowPicker*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (!self || (self->overlay_ != hwnd && msg != WM_NCCREATE && msg != WM_CREATE)) {
        return ::DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return self->HandleOverlay(hwnd, msg, wParam, lParam);
}

LRESULT WindowPicker::HandleOverlay(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_MOUSEMOVE:
        case WM_LBUTTONUP:
        case WM_RBUTTONUP: {
            POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ::ClientToScreen(hwnd, &pt);
            if (msg == WM_MOUSEMOVE) {
                UpdateHighlight(pt);
            } else {
                UpdateHighlight(pt);
                Finish(msg == WM_LBUTTONUP, pt);
            }
            return 0;
        }
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                POINT pt{};
                Finish(false, pt);
            }
            return 0;
        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE && overlay_) {
                POINT pt{};
                Finish(false, pt);
                return 0;
            }
            break;
        default:
            break;
    }
    return ::DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK WindowPicker::FrameProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_ERASEBKGND) {
        return 1;
    }
    if (msg == WM_NCHITTEST) {
        return HTTRANSPARENT;
    }
    if (msg != WM_PAINT) {
        return ::DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    PAINTSTRUCT ps;
    HDC dc = ::BeginPaint(hwnd, &ps);
    RECT rc;
    ::GetClientRect(hwnd, &rc);
    const UINT dpi = api::GetDpiForWindow(hwnd);
    if (::GetWindowLongPtrW(hwnd, GWLP_USERDATA) == kModeHint) {
        GdiObject<HBRUSH> bg(::CreateSolidBrush(RGB(32, 32, 32)));
        ::FillRect(dc, &rc, bg.Get());
        NONCLIENTMETRICSW metrics;
        GdiObject<HFONT> font;
        if (api::GetNonClientMetricsForDpi(metrics, dpi)) {
            font.Reset(::CreateFontIndirectW(&metrics.lfMessageFont));
        }
        HGDIOBJ old = font ? ::SelectObject(dc, font.Get()) : nullptr;
        ::SetBkMode(dc, TRANSPARENT);
        ::SetTextColor(dc, RGB(255, 255, 255));
        ::DrawTextW(dc, str::kPickerHint, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        if (old) {
            ::SelectObject(dc, old);
        }
    } else {
        GdiObject<HBRUSH> key(::CreateSolidBrush(kColorKey));
        ::FillRect(dc, &rc, key.Get());
        GdiObject<HBRUSH> accent(::CreateSolidBrush(kAccent));
        const int t = api::Scale(3, dpi);
        for (int i = 0; i < t; ++i) {
            ::FrameRect(dc, &rc, accent.Get());
            ::InflateRect(&rc, -1, -1);
        }
    }
    ::EndPaint(hwnd, &ps);
    return 0;
}

}  // namespace snippin
