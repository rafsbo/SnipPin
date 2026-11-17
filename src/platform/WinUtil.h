// Window, process and shell helpers shared across modules.
#pragma once

#include <string>

#include "logic/Identity.h"
#include "platform/Win32.h"

namespace snippin::win {

std::wstring GetWindowTextString(HWND hwnd);
std::wstring GetClassNameString(HWND hwnd);
std::wstring GetProcessImagePath(DWORD pid);
DWORD GetWindowProcessId(HWND hwnd);

// Full path of the running executable / its directory (no trailing slash).
std::wstring GetModulePath();
std::wstring GetModuleDirectory();

// True when |hwnd| belongs to this process.
bool IsOwnWindow(HWND hwnd);

// Root owner-less top-level window for any child or owned window.
HWND GetTopLevel(HWND hwnd);

// Hidden by DWM (UWP background windows, other virtual desktops).
bool IsCloaked(HWND hwnd);

// Desktop, taskbar and similar shell surfaces that must never be modified.
bool IsShellSurface(HWND hwnd);

// A visible, top-level, user-facing window from another process that SnipPin
// may pin or modify.
bool IsCandidateWindow(HWND hwnd);

// Visible window bounds (without the invisible Windows 10 resize borders).
RECT GetVisibleFrame(HWND hwnd);

// Small icon for a window, or nullptr. The icon is owned by the window/class.
HICON GetWindowSmallIcon(HWND hwnd);

// Folder shown by an Explorer window, empty if not an Explorer folder window.
std::wstring GetExplorerFolderPath(HWND hwnd);

// Collects exe path, class, title and folder path for |hwnd|.
logic::WindowIdentity GetIdentity(HWND hwnd);

// Token queries for the current process.
bool IsProcessElevated();
bool IsUiAccessProcess();

// Brings |hwnd| to the foreground even when Windows' foreground lock would
// normally refuse (uses AttachThreadInput as a last resort).
void ForceForeground(HWND hwnd);

// "Very long title" -> "Very long ti..."
std::wstring Ellipsize(const std::wstring& text, size_t maxChars);

// Escapes '&' so text can be used as a menu label.
std::wstring EscapeMenuText(const std::wstring& text);

// Work area of the monitor containing |pt|.
RECT GetWorkAreaFromPoint(POINT pt);

}  // namespace snippin::win
