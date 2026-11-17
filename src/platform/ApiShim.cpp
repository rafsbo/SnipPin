#include "platform/ApiShim.h"

#ifndef LOAD_LIBRARY_SEARCH_SYSTEM32
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x00000800
#endif

namespace snippin::api {
namespace {

using DpiAwarenessContext = HANDLE;
// DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 = ((DPI_AWARENESS_CONTEXT)-4)
const DpiAwarenessContext kPerMonitorAwareV2 = reinterpret_cast<DpiAwarenessContext>(static_cast<INT_PTR>(-4));
constexpr int kProcessPerMonitorDpiAware = 2;  // PROCESS_PER_MONITOR_DPI_AWARE
constexpr int kMdtEffectiveDpi = 0;            // MDT_EFFECTIVE_DPI

using SetProcessDpiAwarenessContextFn = BOOL(WINAPI*)(DpiAwarenessContext);
using SetProcessDpiAwarenessFn = HRESULT(WINAPI*)(int);
using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
using GetDpiForMonitorFn = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);
using SystemParametersInfoForDpiFn = BOOL(WINAPI*)(UINT, UINT, PVOID, UINT, UINT);
using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);

struct Functions {
    SetProcessDpiAwarenessContextFn setProcessDpiAwarenessContext = nullptr;
    SetProcessDpiAwarenessFn setProcessDpiAwareness = nullptr;
    GetDpiForWindowFn getDpiForWindow = nullptr;
    GetDpiForMonitorFn getDpiForMonitor = nullptr;
    SystemParametersInfoForDpiFn systemParametersInfoForDpi = nullptr;
    DWORD major = 6;
    DWORD minor = 1;
    bool initialized = false;
};

Functions g_fn;

template <typename T>
T Resolve(HMODULE module, const char* name) {
    if (!module) {
        return nullptr;
    }
    return reinterpret_cast<T>(reinterpret_cast<void*>(::GetProcAddress(module, name)));
}

}  // namespace

void Initialize() {
    if (g_fn.initialized) {
        return;
    }
    g_fn.initialized = true;

    HMODULE user32 = ::GetModuleHandleW(L"user32.dll");
    g_fn.setProcessDpiAwarenessContext =
        Resolve<SetProcessDpiAwarenessContextFn>(user32, "SetProcessDpiAwarenessContext");
    g_fn.getDpiForWindow = Resolve<GetDpiForWindowFn>(user32, "GetDpiForWindow");
    g_fn.systemParametersInfoForDpi =
        Resolve<SystemParametersInfoForDpiFn>(user32, "SystemParametersInfoForDpi");

    // shcore.dll only exists on Windows 8.1+.
    HMODULE shcore = ::LoadLibraryExW(L"shcore.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!shcore) {
        // LOAD_LIBRARY_SEARCH_SYSTEM32 needs KB2533623 on Windows 7; retry plainly.
        shcore = ::LoadLibraryW(L"shcore.dll");
    }
    g_fn.setProcessDpiAwareness = Resolve<SetProcessDpiAwarenessFn>(shcore, "SetProcessDpiAwareness");
    g_fn.getDpiForMonitor = Resolve<GetDpiForMonitorFn>(shcore, "GetDpiForMonitor");

    HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
    if (auto rtlGetVersion = Resolve<RtlGetVersionFn>(ntdll, "RtlGetVersion")) {
        OSVERSIONINFOW info{};
        info.dwOSVersionInfoSize = sizeof(info);
        if (rtlGetVersion(&info) == 0) {
            g_fn.major = info.dwMajorVersion;
            g_fn.minor = info.dwMinorVersion;
        }
    }
}

void EnableBestDpiAwareness() {
    Initialize();
    if (g_fn.setProcessDpiAwarenessContext &&
        g_fn.setProcessDpiAwarenessContext(kPerMonitorAwareV2)) {
        return;
    }
    if (g_fn.setProcessDpiAwareness &&
        SUCCEEDED(g_fn.setProcessDpiAwareness(kProcessPerMonitorDpiAware))) {
        return;
    }
    ::SetProcessDPIAware();  // Vista+; harmless if the manifest already did it.
}

UINT GetSystemDpi() {
    HDC dc = ::GetDC(nullptr);
    const int dpi = dc ? ::GetDeviceCaps(dc, LOGPIXELSX) : 96;
    if (dc) {
        ::ReleaseDC(nullptr, dc);
    }
    return dpi > 0 ? static_cast<UINT>(dpi) : 96;
}

UINT GetDpiForWindow(HWND hwnd) {
    Initialize();
    if (hwnd && g_fn.getDpiForWindow) {
        const UINT dpi = g_fn.getDpiForWindow(hwnd);
        if (dpi) {
            return dpi;
        }
    }
    if (hwnd && g_fn.getDpiForMonitor) {
        UINT x = 0;
        UINT y = 0;
        HMONITOR monitor = ::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        if (SUCCEEDED(g_fn.getDpiForMonitor(monitor, kMdtEffectiveDpi, &x, &y)) && x) {
            return x;
        }
    }
    return GetSystemDpi();
}

UINT GetMonitorDpi(HWND hwnd) {
    Initialize();
    if (hwnd && g_fn.getDpiForMonitor) {
        UINT x = 0;
        UINT y = 0;
        HMONITOR monitor = ::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        if (SUCCEEDED(g_fn.getDpiForMonitor(monitor, kMdtEffectiveDpi, &x, &y)) && x) {
            return x;
        }
    }
    return GetSystemDpi();
}

bool IsWindows8OrGreater() {
    Initialize();
    return g_fn.major > 6 || (g_fn.major == 6 && g_fn.minor >= 2);
}

bool IsWindows81OrGreater() {
    Initialize();
    return g_fn.major > 6 || (g_fn.major == 6 && g_fn.minor >= 3);
}

bool IsWindows10OrGreater() {
    Initialize();
    return g_fn.major >= 10;
}

bool GetNonClientMetricsForDpi(NONCLIENTMETRICSW& metrics, UINT dpi) {
    Initialize();
    metrics = NONCLIENTMETRICSW{};
    metrics.cbSize = sizeof(metrics);
    if (g_fn.systemParametersInfoForDpi &&
        g_fn.systemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, metrics.cbSize, &metrics, 0, dpi)) {
        return true;
    }
    if (!::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, metrics.cbSize, &metrics, 0)) {
        return false;
    }
    // Legacy path returns system-DPI values; rescale fonts for the target DPI.
    const UINT systemDpi = GetSystemDpi();
    if (systemDpi != dpi) {
        metrics.lfMessageFont.lfHeight = MulDiv(metrics.lfMessageFont.lfHeight, dpi, systemDpi);
        metrics.lfMenuFont.lfHeight = MulDiv(metrics.lfMenuFont.lfHeight, dpi, systemDpi);
        metrics.lfCaptionFont.lfHeight = MulDiv(metrics.lfCaptionFont.lfHeight, dpi, systemDpi);
        metrics.lfStatusFont.lfHeight = MulDiv(metrics.lfStatusFont.lfHeight, dpi, systemDpi);
    }
    return true;
}

bool IsCompositionEnabled() {
    if (IsWindows8OrGreater()) {
        return true;
    }
    BOOL enabled = FALSE;
    return SUCCEEDED(::DwmIsCompositionEnabled(&enabled)) && enabled;
}

}  // namespace snippin::api
