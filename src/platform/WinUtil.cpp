#include "platform/WinUtil.h"

#include <exdisp.h>
#include <shlobj.h>
#include <shlwapi.h>

#include <iterator>
#include <vector>

namespace snippin::win {

std::wstring GetWindowTextString(HWND hwnd) {
    // InternalGetWindowText never sends messages, so a hung target cannot
    // block SnipPin.
    wchar_t buffer[512] = {};
    const int len = ::InternalGetWindowText(hwnd, buffer, static_cast<int>(std::size(buffer)));
    return std::wstring(buffer, len > 0 ? static_cast<size_t>(len) : 0);
}

std::wstring GetClassNameString(HWND hwnd) {
    wchar_t buffer[256] = {};
    const int len = ::GetClassNameW(hwnd, buffer, static_cast<int>(std::size(buffer)));
    return std::wstring(buffer, len > 0 ? static_cast<size_t>(len) : 0);
}

std::wstring GetProcessImagePath(DWORD pid) {
    HANDLE process = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) {
        return {};
    }
    std::vector<wchar_t> buffer(MAX_PATH);
    std::wstring result;
    for (int attempt = 0; attempt < 4; ++attempt) {
        DWORD size = static_cast<DWORD>(buffer.size());
        if (::QueryFullProcessImageNameW(process, 0, buffer.data(), &size)) {
            result.assign(buffer.data(), size);
            break;
        }
        if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            break;
        }
        buffer.resize(buffer.size() * 2);
    }
    ::CloseHandle(process);
    return result;
}

DWORD GetWindowProcessId(HWND hwnd) {
    DWORD pid = 0;
    ::GetWindowThreadProcessId(hwnd, &pid);
    return pid;
}

std::wstring GetModulePath() {
    std::vector<wchar_t> buffer(MAX_PATH);
    for (;;) {
        const DWORD len = ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (len == 0) {
            return {};
        }
        if (len < buffer.size()) {
            return std::wstring(buffer.data(), len);
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::wstring GetModuleDirectory() {
    std::wstring path = GetModulePath();
    const size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring(L".") : path.substr(0, slash);
}

bool IsOwnWindow(HWND hwnd) {
    return hwnd && GetWindowProcessId(hwnd) == ::GetCurrentProcessId();
}

HWND GetTopLevel(HWND hwnd) {
    if (!hwnd) {
        return nullptr;
    }
    HWND root = ::GetAncestor(hwnd, GA_ROOT);
    return root ? root : hwnd;
}

bool IsCloaked(HWND hwnd) {
    DWORD cloaked = 0;
    // Fails harmlessly on Windows 7, where cloaking does not exist.
    return SUCCEEDED(::DwmGetWindowAttribute(hwnd, kDwmwaCloaked, &cloaked, sizeof(cloaked))) &&
           cloaked != 0;
}

bool IsShellSurface(HWND hwnd) {
    if (hwnd == ::GetDesktopWindow() || hwnd == ::GetShellWindow()) {
        return true;
    }
    const std::wstring cls = GetClassNameString(hwnd);
    static const wchar_t* const kShellClasses[] = {
        L"Progman", L"WorkerW", L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd",
        L"NotifyIconOverflowWindow", L"DV2ControlHost", L"Windows.UI.Core.CoreWindow",
        L"MultitaskingViewFrame", L"TaskListThumbnailWnd", L"XamlExplorerHostIslandWindow",
    };
    for (const wchar_t* name : kShellClasses) {
        if (cls == name) {
            return true;
        }
    }
    return false;
}

bool IsCandidateWindow(HWND hwnd) {
    if (!hwnd || !::IsWindow(hwnd) || !::IsWindowVisible(hwnd)) {
        return false;
    }
    if (::GetAncestor(hwnd, GA_ROOT) != hwnd) {
        return false;  // child window
    }
    if (IsOwnWindow(hwnd) || IsShellSurface(hwnd) || IsCloaked(hwnd)) {
        return false;
    }
    const LONG_PTR style = ::GetWindowLongPtrW(hwnd, GWL_STYLE);
    if (style & WS_CHILD) {
        return false;
    }
    RECT rc{};
    if (!::GetWindowRect(hwnd, &rc) || rc.right <= rc.left || rc.bottom <= rc.top) {
        return false;
    }
    return true;
}

RECT GetVisibleFrame(HWND hwnd) {
    RECT rc{};
    if (FAILED(::DwmGetWindowAttribute(hwnd, kDwmwaExtendedFrameBounds, &rc, sizeof(rc))) ||
        rc.right <= rc.left) {
        ::GetWindowRect(hwnd, &rc);
    }
    return rc;
}

HICON GetWindowSmallIcon(HWND hwnd) {
    DWORD_PTR result = 0;
    const UINT flags = SMTO_ABORTIFHUNG | SMTO_BLOCK;
    if (::SendMessageTimeoutW(hwnd, WM_GETICON, ICON_SMALL2, 0, flags, 100, &result) && result) {
        return reinterpret_cast<HICON>(result);
    }
    if (::SendMessageTimeoutW(hwnd, WM_GETICON, ICON_SMALL, 0, flags, 100, &result) && result) {
        return reinterpret_cast<HICON>(result);
    }
    if (auto icon = reinterpret_cast<HICON>(::GetClassLongPtrW(hwnd, GCLP_HICONSM))) {
        return icon;
    }
    if (::SendMessageTimeoutW(hwnd, WM_GETICON, ICON_BIG, 0, flags, 100, &result) && result) {
        return reinterpret_cast<HICON>(result);
    }
    return reinterpret_cast<HICON>(::GetClassLongPtrW(hwnd, GCLP_HICON));
}

std::wstring GetExplorerFolderPath(HWND hwnd) {
    if (GetClassNameString(hwnd) != L"CabinetWClass") {
        return {};
    }
    std::wstring result;
    IShellWindows* windows = nullptr;
    if (FAILED(::CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_ALL, IID_IShellWindows,
                                  reinterpret_cast<void**>(&windows))) ||
        !windows) {
        return {};
    }
    long count = 0;
    windows->get_Count(&count);
    for (long i = 0; i < count && result.empty(); ++i) {
        VARIANT index;
        ::VariantInit(&index);
        index.vt = VT_I4;
        index.lVal = i;
        IDispatch* dispatch = nullptr;
        if (FAILED(windows->Item(index, &dispatch)) || !dispatch) {
            continue;
        }
        IWebBrowserApp* browser = nullptr;
        if (SUCCEEDED(dispatch->QueryInterface(IID_IWebBrowserApp, reinterpret_cast<void**>(&browser))) &&
            browser) {
            SHANDLE_PTR handle = 0;
            if (SUCCEEDED(browser->get_HWND(&handle)) &&
                reinterpret_cast<HWND>(static_cast<LONG_PTR>(handle)) == hwnd) {
                BSTR url = nullptr;
                if (SUCCEEDED(browser->get_LocationURL(&url)) && url && *url) {
                    wchar_t path[MAX_PATH * 4] = {};
                    DWORD size = static_cast<DWORD>(std::size(path));
                    if (SUCCEEDED(::PathCreateFromUrlW(url, path, &size, 0))) {
                        result.assign(path, size);
                    }
                }
                if (url) {
                    ::SysFreeString(url);
                }
            }
            browser->Release();
        }
        dispatch->Release();
    }
    windows->Release();
    return result;
}

logic::WindowIdentity GetIdentity(HWND hwnd) {
    logic::WindowIdentity id;
    id.exePath = GetProcessImagePath(GetWindowProcessId(hwnd));
    id.className = GetClassNameString(hwnd);
    id.title = GetWindowTextString(hwnd);
    id.folderPath = GetExplorerFolderPath(hwnd);
    return id;
}

namespace {

bool QueryTokenDword(TOKEN_INFORMATION_CLASS cls) {
    HANDLE token = nullptr;
    if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return false;
    }
    DWORD value = 0;
    DWORD size = 0;
    const BOOL ok = ::GetTokenInformation(token, cls, &value, sizeof(value), &size);
    ::CloseHandle(token);
    return ok && value != 0;
}

}  // namespace

bool IsProcessElevated() { return QueryTokenDword(TokenElevation); }

bool IsUiAccessProcess() { return QueryTokenDword(TokenUIAccess); }

void ForceForeground(HWND hwnd) {
    if (!hwnd || !::IsWindow(hwnd)) {
        return;
    }
    if (::IsIconic(hwnd)) {
        ::ShowWindow(hwnd, SW_RESTORE);
    }
    if (::SetForegroundWindow(hwnd) && ::GetForegroundWindow() == hwnd) {
        return;
    }
    // Foreground lock: temporarily share input state with the current
    // foreground thread so the request is honoured.
    HWND foreground = ::GetForegroundWindow();
    const DWORD foregroundThread = foreground ? ::GetWindowThreadProcessId(foreground, nullptr) : 0;
    const DWORD currentThread = ::GetCurrentThreadId();
    const bool attached = foregroundThread && foregroundThread != currentThread &&
                          ::AttachThreadInput(currentThread, foregroundThread, TRUE);
    ::BringWindowToTop(hwnd);
    ::SetForegroundWindow(hwnd);
    if (attached) {
        ::AttachThreadInput(currentThread, foregroundThread, FALSE);
    }
}

std::wstring Ellipsize(const std::wstring& text, size_t maxChars) {
    if (text.size() <= maxChars || maxChars < 2) {
        return text;
    }
    return text.substr(0, maxChars - 1) + L"\u2026";
}

std::wstring EscapeMenuText(const std::wstring& text) {
    std::wstring out;
    out.reserve(text.size());
    for (wchar_t c : text) {
        if (c == L'&') {
            out += L"&&";
        } else if (c == L'\t') {
            out += L' ';
        } else {
            out += c;
        }
    }
    return out;
}

RECT GetWorkAreaFromPoint(POINT pt) {
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    ::GetMonitorInfoW(::MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST), &info);
    return info.rcWork;
}

}  // namespace snippin::win
