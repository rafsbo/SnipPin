#include "snippet/SnippetWindow.h"

#include <algorithm>
#include <cmath>

#include "app/AppInfo.h"
#include "i18n/Catalog.h"
#include "platform/ApiShim.h"
#include "platform/Log.h"
#include "platform/WinUtil.h"

namespace snippin {

using i18n::StringId;
using i18n::Tr;
using i18n::TrFormat;
namespace {

constexpr wchar_t kClassName[] = L"SnipPin.Snippet";
constexpr COLORREF kBackground = RGB(32, 32, 32);
constexpr COLORREF kBorderLive = RGB(0, 120, 215);
constexpr COLORREF kBorderSnapshot = RGB(202, 80, 16);
constexpr DWORD kStyle = WS_POPUP | WS_CLIPCHILDREN;
constexpr DWORD kExStyle = WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;

void RegisterClassOnce(HINSTANCE instance, WNDPROC proc) {
    static bool registered = false;
    if (registered) {
        return;
    }
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = proc;
    wc.hInstance = instance;
    wc.hCursor = ::LoadCursorW(nullptr, IDC_SIZEALL);
    wc.lpszClassName = kClassName;
    registered = ::RegisterClassExW(&wc) != 0;
}

logic::Size WindowSize(HWND hwnd) {
    RECT rc{};
    ::GetWindowRect(hwnd, &rc);
    return logic::Size{rc.right - rc.left, rc.bottom - rc.top};
}

}  // namespace

SnippetWindow::SnippetWindow(std::uint64_t id, Callbacks callbacks) : id_(id), callbacks_(std::move(callbacks)) {}

SnippetWindow::~SnippetWindow() { Destroy(); }

bool SnippetWindow::Create(HINSTANCE instance, HWND source, const logic::Rect& regionInWindow,
                           const logic::Rect& screenRegion, const SnippetOptions& options) {
    RegisterClassOnce(instance, &SnippetWindow::WndProc);
    source_ = source;
    sourceTitle_ = win::GetWindowTextString(source);
    region_ = regionInWindow;
    baseSize_ = logic::Size{std::max(1, screenRegion.Width()), std::max(1, screenRegion.Height())};
    options_ = options;
    live_ = options.live;

    // Frozen snapshots are taken from the screen right now: exactly what the
    // user saw when selecting, even for GPU-rendered content.
    if (!live_) {
        CaptureFromScreen(screenRegion);
    }

    const int border = api::Scale(1, api::GetSystemDpi());
    logic::Rect windowRect{screenRegion.left - border, screenRegion.top - border, screenRegion.right + border,
                           screenRegion.bottom + border};
    // Offset slightly so the snippet does not hide the area it mirrors.
    const int offset = api::Scale(24, api::GetSystemDpi());
    windowRect = logic::OffsetRect(windowRect, offset, offset);
    const POINT center{windowRect.left + windowRect.Width() / 2, windowRect.top + windowRect.Height() / 2};
    windowRect = logic::FitInside(windowRect, ToRect(win::GetWorkAreaFromPoint(center)));

    ::CreateWindowExW(kExStyle, kClassName, i18n::TrFormat(i18n::StringId::SnippetTitle, {sourceTitle_}).c_str(), kStyle,
                      windowRect.left, windowRect.top, windowRect.Width(), windowRect.Height(), nullptr, nullptr,
                      instance, this);
    if (!hwnd_) {
        SNIPPIN_LOG(L"Snippet window creation failed (error %lu)", ::GetLastError());
        return false;
    }
    ResizeToZoom();  // re-applies borders at the snippet's own DPI
    ChooseRenderer();
    ::ShowWindow(hwnd_, SW_SHOWNOACTIVATE);  // ChooseRenderer() already started the timer
    return true;
}

void SnippetWindow::Destroy() {
    if (!hwnd_) {
        return;
    }
    UnregisterThumbnail();
    HWND hwnd = hwnd_;
    hwnd_ = nullptr;
    ::DestroyWindow(hwnd);
}

// ------------------------------------------------------------------ rendering

void SnippetWindow::ChooseRenderer() {
    UnregisterThumbnail();
    if (!live_) {
        renderer_ = Renderer::Frozen;
    } else if (sourceClosed_) {
        renderer_ = frame_ ? Renderer::Frozen : Renderer::None;
    } else if (!options_.compatibilityMode && api::IsCompositionEnabled()) {
        renderer_ = Renderer::Thumbnail;
        RegisterThumbnail();
        if (!thumbnail_) {
            renderer_ = Renderer::PrintWindow;
        }
    } else {
        renderer_ = Renderer::PrintWindow;
    }

    if (hwnd_) {
        const UINT interval = renderer_ == Renderer::PrintWindow ? static_cast<UINT>(options_.fallbackIntervalMs)
                                                                 : kSyncIntervalMs;
        ::SetTimer(hwnd_, kSyncTimer, interval, nullptr);
        if (renderer_ == Renderer::PrintWindow) {
            CaptureWithPrintWindow();
        }
        ::InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void SnippetWindow::RegisterThumbnail() {
    if (thumbnail_ || !hwnd_ || !::IsWindow(source_)) {
        return;
    }
    if (FAILED(::DwmRegisterThumbnail(hwnd_, source_, &thumbnail_))) {
        thumbnail_ = nullptr;
        SNIPPIN_LOG(L"DwmRegisterThumbnail failed for source %p", source_);
        return;
    }
    UpdateThumbnail();
}

void SnippetWindow::UnregisterThumbnail() {
    if (thumbnail_) {
        ::DwmUnregisterThumbnail(thumbnail_);
        thumbnail_ = nullptr;
    }
}

void SnippetWindow::UpdateThumbnail() {
    if (!thumbnail_) {
        return;
    }
    SIZE surface{};
    ::DwmQueryThumbnailSourceSize(thumbnail_, &surface);
    const logic::Size windowSize = WindowSize(source_);
    lastSourceSize_ = windowSize;
    const logic::Rect src =
        logic::ScaleRegionToSurface(region_, windowSize, logic::Size{surface.cx, surface.cy});

    DWM_THUMBNAIL_PROPERTIES props{};
    props.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_RECTSOURCE | DWM_TNP_VISIBLE | DWM_TNP_OPACITY |
                    DWM_TNP_SOURCECLIENTAREAONLY;
    props.rcDestination = ContentRect();
    props.rcSource = ToRECT(src);
    props.opacity = 255;  // window-level opacity comes from the layered style
    props.fVisible = (!sourceMinimized_ && !src.IsEmpty()) ? TRUE : FALSE;
    props.fSourceClientAreaOnly = FALSE;
    ::DwmUpdateThumbnailProperties(thumbnail_, &props);
}

bool SnippetWindow::CaptureWithPrintWindow() {
    if (!::IsWindow(source_) || ::IsIconic(source_)) {
        return false;
    }
    const logic::Size windowSize = WindowSize(source_);
    if (windowSize.cx <= 0 || windowSize.cy <= 0) {
        return false;
    }
    // DPI-unaware windows render at their logical size.
    const UINT windowDpi = api::GetDpiForWindow(source_);
    const UINT monitorDpi = api::GetMonitorDpi(source_);
    logic::Size surface = windowSize;
    if (windowDpi && monitorDpi && windowDpi != monitorDpi) {
        surface = logic::Size{MulDiv(windowSize.cx, windowDpi, monitorDpi), MulDiv(windowSize.cy, windowDpi, monitorDpi)};
    }

    HDC screen = ::GetDC(nullptr);
    HDC mem = ::CreateCompatibleDC(screen);
    GdiObject<HBITMAP> bitmap(::CreateCompatibleBitmap(screen, surface.cx, surface.cy));
    HGDIOBJ old = ::SelectObject(mem, bitmap.Get());
    const UINT flags = api::IsWindows81OrGreater() ? kPwRenderFullContent : 0;
    const BOOL ok = ::PrintWindow(source_, mem, flags);
    ::SelectObject(mem, old);
    ::DeleteDC(mem);
    ::ReleaseDC(nullptr, screen);
    if (!ok) {
        return false;
    }
    frame_ = std::move(bitmap);
    frameCrop_ = logic::ScaleRegionToSurface(region_, windowSize, surface);
    return true;
}

bool SnippetWindow::CaptureFromScreen(const logic::Rect& screenRegion) {
    if (screenRegion.IsEmpty()) {
        return false;
    }
    ::DwmFlush();  // let DWM finish removing the selection overlay
    HDC screen = ::GetDC(nullptr);
    HDC mem = ::CreateCompatibleDC(screen);
    GdiObject<HBITMAP> bitmap(::CreateCompatibleBitmap(screen, screenRegion.Width(), screenRegion.Height()));
    HGDIOBJ old = ::SelectObject(mem, bitmap.Get());
    const BOOL ok = ::BitBlt(mem, 0, 0, screenRegion.Width(), screenRegion.Height(), screen, screenRegion.left,
                             screenRegion.top, SRCCOPY | CAPTUREBLT);
    ::SelectObject(mem, old);
    ::DeleteDC(mem);
    ::ReleaseDC(nullptr, screen);
    if (!ok) {
        return false;
    }
    frame_ = std::move(bitmap);
    frameCrop_ = logic::Rect{0, 0, screenRegion.Width(), screenRegion.Height()};
    return true;
}

int SnippetWindow::Border() const { return std::max(1, api::Scale(1, api::GetDpiForWindow(hwnd_))); }

RECT SnippetWindow::ContentRect() const {
    RECT rc{};
    ::GetClientRect(hwnd_, &rc);
    const int b = Border();
    ::InflateRect(&rc, -b, -b);
    return rc;
}

void SnippetWindow::Paint(HDC dc) {
    RECT client;
    ::GetClientRect(hwnd_, &client);
    HDC mem = ::CreateCompatibleDC(dc);
    GdiObject<HBITMAP> buffer(::CreateCompatibleBitmap(dc, client.right, client.bottom));
    HGDIOBJ oldBuffer = ::SelectObject(mem, buffer.Get());

    GdiObject<HBRUSH> border(::CreateSolidBrush(live_ ? kBorderLive : kBorderSnapshot));
    ::FillRect(mem, &client, border.Get());
    const RECT content = ContentRect();
    GdiObject<HBRUSH> background(::CreateSolidBrush(kBackground));
    ::FillRect(mem, &content, background.Get());

    const wchar_t* message = nullptr;
    const bool drawFrame = (renderer_ == Renderer::Frozen || renderer_ == Renderer::PrintWindow) && frame_;
    if (drawFrame && !frameCrop_.IsEmpty()) {
        HDC src = ::CreateCompatibleDC(dc);
        HGDIOBJ oldSrc = ::SelectObject(src, frame_.Get());
        ::SetStretchBltMode(mem, HALFTONE);
        ::SetBrushOrgEx(mem, 0, 0, nullptr);
        ::StretchBlt(mem, content.left, content.top, content.right - content.left, content.bottom - content.top, src,
                     frameCrop_.left, frameCrop_.top, frameCrop_.Width(), frameCrop_.Height(), SRCCOPY);
        ::SelectObject(src, oldSrc);
        ::DeleteDC(src);
    }
    if (sourceClosed_ && !(renderer_ == Renderer::Frozen && frame_)) {
        message = Tr(StringId::SnippetSourceClosed);
    } else if (sourceMinimized_ && live_) {
        message = Tr(StringId::SnippetSourceMinimized);
    } else if (renderer_ != Renderer::Thumbnail && !drawFrame) {
        message = Tr(StringId::SnippetNoContent);
    }
    if (message) {
        ::SetBkMode(mem, TRANSPARENT);
        ::SetTextColor(mem, RGB(220, 220, 220));
        NONCLIENTMETRICSW metrics;
        GdiObject<HFONT> font;
        if (api::GetNonClientMetricsForDpi(metrics, api::GetDpiForWindow(hwnd_))) {
            font.Reset(::CreateFontIndirectW(&metrics.lfMessageFont));
        }
        HGDIOBJ oldFont = font ? ::SelectObject(mem, font.Get()) : nullptr;
        RECT textRect = content;
        ::DrawTextW(mem, message, -1, &textRect, DT_CENTER | DT_VCENTER | DT_WORDBREAK | DT_END_ELLIPSIS);
        if (oldFont) {
            ::SelectObject(mem, oldFont);
        }
    }

    ::BitBlt(dc, 0, 0, client.right, client.bottom, mem, 0, 0, SRCCOPY);
    ::SelectObject(mem, oldBuffer);
    ::DeleteDC(mem);
}

// ------------------------------------------------------------------ state

void SnippetWindow::ApplyLayeredStyle() {
    if (!hwnd_) {
        return;
    }
    const bool needLayered = ghost_ || opacityPercent_ < 100;
    LONG_PTR ex = ::GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
    LONG_PTR desired = ex;
    desired = needLayered ? (desired | WS_EX_LAYERED) : (desired & ~static_cast<LONG_PTR>(WS_EX_LAYERED));
    desired = ghost_ ? (desired | WS_EX_TRANSPARENT) : (desired & ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT));
    if (desired != ex) {
        ::SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, desired);
    }
    if (needLayered) {
        ::SetLayeredWindowAttributes(hwnd_, 0, logic::PercentToAlpha(opacityPercent_), LWA_ALPHA);
    }
    ::RedrawWindow(hwnd_, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME);
}

void SnippetWindow::ResizeToZoom() {
    if (!hwnd_) {
        return;
    }
    const logic::Size content = logic::ZoomSize(baseSize_, zoom_);
    const int b = Border();
    ::SetWindowPos(hwnd_, nullptr, 0, 0, content.cx + 2 * b, content.cy + 2 * b,
                   SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    UpdateThumbnail();
    ::InvalidateRect(hwnd_, nullptr, FALSE);
}

void SnippetWindow::SetLive(bool live) {
    if (live == live_) {
        return;
    }
    if (!live) {
        // Freeze what the snippet currently shows.
        if (renderer_ != Renderer::PrintWindow || !frame_) {
            CaptureWithPrintWindow();
        }
    }
    live_ = live;
    ChooseRenderer();
    Changed();
}

void SnippetWindow::RefreshSnapshot() {
    if (CaptureWithPrintWindow()) {
        ::InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void SnippetWindow::SetOpacity(int percent, int minPercent) {
    opacityPercent_ = logic::ClampOpacityPercent(percent, minPercent);
    ApplyLayeredStyle();
    Changed();
}

void SnippetWindow::SetGhost(bool ghost, int ghostOpacityPercent, int minPercent, std::uint64_t sequence) {
    ghost_ = ghost;
    if (ghost) {
        ghostSequence_ = sequence;
        if (opacityPercent_ >= 100) {
            opacityPercent_ = logic::ClampOpacityPercent(ghostOpacityPercent, minPercent);
        }
    }
    ApplyLayeredStyle();
    Changed();
}

void SnippetWindow::SetZoom(double zoom) {
    zoom_ = logic::ClampZoom(zoom);
    ResizeToZoom();
}

void SnippetWindow::OnWheel(int delta, bool ctrl, int opacityStep, int minPercent) {
    const int direction = delta > 0 ? 1 : -1;
    if (ctrl) {
        SetOpacity(opacityPercent_ + direction * opacityStep, minPercent);
    } else {
        SetZoom(zoom_ * (direction > 0 ? 1.1 : 1.0 / 1.1));
    }
}

void SnippetWindow::SetOptions(const SnippetOptions& options) {
    const bool rendererChanged = options.compatibilityMode != options_.compatibilityMode ||
                                 options.fallbackIntervalMs != options_.fallbackIntervalMs;
    const bool keepLive = live_;
    options_ = options;
    options_.live = keepLive;
    if (rendererChanged) {
        ChooseRenderer();
    }
}

void SnippetWindow::FocusSource() {
    if (::IsWindow(source_)) {
        win::ForceForeground(source_);
    }
}

void SnippetWindow::ReassertTopmost() {
    if (hwnd_) {
        ::SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

void SnippetWindow::OnSourceDestroyed() {
    sourceClosed_ = true;
    UnregisterThumbnail();
    if (frame_) {
        live_ = false;  // keep the last captured pixels as a snapshot
    }
    ChooseRenderer();
    Changed();
}

void SnippetWindow::OnCompositionChanged() {
    if (live_) {
        ChooseRenderer();
    }
}

void SnippetWindow::Changed() {
    if (callbacks_.onChanged) {
        callbacks_.onChanged(*this);
    }
}

// ------------------------------------------------------------------ window

LRESULT SnippetWindow::HitTest(POINT pt) const {
    RECT rc;
    ::GetWindowRect(hwnd_, &rc);
    const int grip = api::Scale(6, api::GetDpiForWindow(hwnd_));
    const bool left = pt.x < rc.left + grip;
    const bool right = pt.x >= rc.right - grip;
    const bool top = pt.y < rc.top + grip;
    const bool bottom = pt.y >= rc.bottom - grip;
    if (top && left) return HTTOPLEFT;
    if (top && right) return HTTOPRIGHT;
    if (bottom && left) return HTBOTTOMLEFT;
    if (bottom && right) return HTBOTTOMRIGHT;
    if (left) return HTLEFT;
    if (right) return HTRIGHT;
    if (top) return HTTOP;
    if (bottom) return HTBOTTOM;
    return HTCAPTION;  // drag anywhere to move
}

LRESULT CALLBACK SnippetWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SnippetWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        self = static_cast<SnippetWindow*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<SnippetWindow*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (!self || self->hwnd_ != hwnd) {
        if (msg == WM_NCDESTROY) {
            ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        }
        return ::DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return self->HandleMessage(msg, wParam, lParam);
}

LRESULT SnippetWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
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
        case WM_NCHITTEST:
            return HitTest(POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCAPTION) {
                ::SetCursor(::LoadCursorW(nullptr, IDC_SIZEALL));
                return TRUE;
            }
            break;
        case WM_SIZING: {
            auto* rc = reinterpret_cast<RECT*>(lParam);
            const int b = Border();
            const logic::Size requested{rc->right - rc->left - 2 * b, rc->bottom - rc->top - 2 * b};
            const bool widthDriven = wParam != WMSZ_TOP && wParam != WMSZ_BOTTOM;
            const logic::Size fitted = logic::KeepAspect(baseSize_, requested, widthDriven);
            const double zoom = logic::ClampZoom(static_cast<double>(fitted.cx) / baseSize_.cx);
            const logic::Size content = logic::ZoomSize(baseSize_, zoom);
            const int w = content.cx + 2 * b;
            const int h = content.cy + 2 * b;
            if (wParam == WMSZ_LEFT || wParam == WMSZ_TOPLEFT || wParam == WMSZ_BOTTOMLEFT) {
                rc->left = rc->right - w;
            } else {
                rc->right = rc->left + w;
            }
            if (wParam == WMSZ_TOP || wParam == WMSZ_TOPLEFT || wParam == WMSZ_TOPRIGHT) {
                rc->top = rc->bottom - h;
            } else {
                rc->bottom = rc->top + h;
            }
            zoom_ = zoom;
            return TRUE;
        }
        case WM_SIZE:
            UpdateThumbnail();
            ::InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        case WM_NCLBUTTONDBLCLK:
            FocusSource();
            return 0;
        case WM_CONTEXTMENU: {
            // DefWindowProc sends this for right-clicks on our HTCAPTION area.
            POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            if (pt.x == -1 && pt.y == -1) {
                ::GetCursorPos(&pt);
            }
            if (callbacks_.onContextMenu) {
                callbacks_.onContextMenu(*this, pt);
            }
            return 0;
        }
        case WM_MOUSEWHEEL:
            // Reaches us on Windows 10+ ("scroll inactive windows"); on older
            // systems the app's low-level mouse hook forwards wheel events.
            OnWheel(GET_WHEEL_DELTA_WPARAM(wParam), (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL) != 0,
                    options_.opacityStepPercent, options_.minOpacityPercent);
            return 0;
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_TIMER:
            if (wParam == kSyncTimer) {
                const bool minimized = ::IsWindow(source_) && ::IsIconic(source_);
                const bool minimizedChanged = minimized != sourceMinimized_;
                if (minimizedChanged) {
                    sourceMinimized_ = minimized;
                    ::InvalidateRect(hwnd_, nullptr, FALSE);
                }
                if (renderer_ == Renderer::Thumbnail) {
                    // The crop must be rescaled when the source is resized.
                    if (minimizedChanged || !(WindowSize(source_) == lastSourceSize_)) {
                        UpdateThumbnail();
                    }
                } else if (renderer_ == Renderer::PrintWindow && !minimized) {
                    if (CaptureWithPrintWindow()) {
                        ::InvalidateRect(hwnd_, nullptr, FALSE);
                    }
                }
            }
            return 0;
        case kWmDpiChanged:
            // Keep the mirrored pixels 1:1; only borders change.
            ResizeToZoom();
            return 0;
        case WM_DWMCOMPOSITIONCHANGED:
            OnCompositionChanged();
            return 0;
        case WM_CLOSE:
            if (callbacks_.onCloseRequested) {
                callbacks_.onCloseRequested(*this);
            }
            return 0;
        default:
            break;
    }
    return ::DefWindowProcW(hwnd_, msg, wParam, lParam);
}

}  // namespace snippin
