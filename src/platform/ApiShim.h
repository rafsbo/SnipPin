// Runtime-resolved access to APIs newer than Windows 7. Every function here is
// safe to call on Windows 7: it falls back gracefully when the API is missing.
#pragma once

#include "platform/Win32.h"

namespace snippin::api {

// Resolves optional entry points. Call once at startup.
void Initialize();

// Opts the process into the best available DPI awareness (PerMonitorV2 ->
// PerMonitor -> System). The manifest normally does this already.
void EnableBestDpiAwareness();

// DPI of the monitor/window (96 = 100 %).
UINT GetDpiForWindow(HWND hwnd);
UINT GetSystemDpi();
// Effective DPI of the monitor that shows |hwnd| (ignores the window's own
// awareness). Compared with GetDpiForWindow() it tells whether Windows is
// bitmap-stretching a DPI-unaware window.
UINT GetMonitorDpi(HWND hwnd);

// Scales a 96-DPI design value to |dpi|.
inline int Scale(int value, UINT dpi) { return MulDiv(value, static_cast<int>(dpi), 96); }

// Reports the OS version (RtlGetVersion, not subject to manifest lies).
bool IsWindows8OrGreater();
bool IsWindows81OrGreater();
bool IsWindows10OrGreater();

// SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS) with a fallback.
bool GetNonClientMetricsForDpi(NONCLIENTMETRICSW& metrics, UINT dpi);

// True when DWM composition is active (always true on Windows 8+).
bool IsCompositionEnabled();

}  // namespace snippin::api
