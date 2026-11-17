// Single entry point for Windows headers. The target baseline is Windows 7
// (0x0601); anything newer is resolved at runtime through ApiShim.
#pragma once

#ifndef WINVER
#define WINVER 0x0601
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>

#include "logic/Geometry.h"

namespace snippin {

// Constants that the Windows 7 SDK level hides or does not define.
constexpr DWORD kDwmwaCloaked = 14;                 // DWMWA_CLOAKED (Windows 8+)
constexpr DWORD kDwmwaExtendedFrameBounds = 9;      // DWMWA_EXTENDED_FRAME_BOUNDS (Vista+)
constexpr UINT kPwRenderFullContent = 0x00000002;   // PW_RENDERFULLCONTENT (Windows 8.1+)
constexpr UINT kWmDpiChanged = 0x02E0;              // WM_DPICHANGED (Windows 8.1+)

inline logic::Rect ToRect(const RECT& r) { return logic::Rect{r.left, r.top, r.right, r.bottom}; }
inline RECT ToRECT(const logic::Rect& r) { return RECT{r.left, r.top, r.right, r.bottom}; }

// RAII wrapper for GDI objects.
template <typename T>
class GdiObject {
public:
    GdiObject() = default;
    explicit GdiObject(T handle) : handle_(handle) {}
    ~GdiObject() { Reset(); }
    GdiObject(const GdiObject&) = delete;
    GdiObject& operator=(const GdiObject&) = delete;
    GdiObject(GdiObject&& o) noexcept : handle_(o.handle_) { o.handle_ = nullptr; }
    GdiObject& operator=(GdiObject&& o) noexcept {
        if (this != &o) {
            Reset();
            handle_ = o.handle_;
            o.handle_ = nullptr;
        }
        return *this;
    }
    void Reset(T handle = nullptr) {
        if (handle_) {
            ::DeleteObject(handle_);
        }
        handle_ = handle;
    }
    T Get() const { return handle_; }
    explicit operator bool() const { return handle_ != nullptr; }

private:
    T handle_ = nullptr;
};

}  // namespace snippin
