#include "platform/Log.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include "platform/Win32.h"

namespace snippin::log {
namespace {

std::wstring g_path;
bool g_toFile = false;

}  // namespace

void Initialize(const std::wstring& filePath, bool toFile) {
    g_path = filePath;
    g_toFile = toFile && !filePath.empty();
}

void Write(const wchar_t* format, ...) {
    wchar_t message[1024];
    va_list args;
    va_start(args, format);
    _vsnwprintf_s(message, _countof(message), _TRUNCATE, format, args);
    va_end(args);

    SYSTEMTIME now;
    ::GetLocalTime(&now);
    wchar_t line[1200];
    _snwprintf_s(line, _countof(line), _TRUNCATE, L"[SnipPin %02u:%02u:%02u.%03u] %s\r\n", now.wHour,
                 now.wMinute, now.wSecond, now.wMilliseconds, message);
    ::OutputDebugStringW(line);

    if (!g_toFile) {
        return;
    }
    HANDLE file = ::CreateFileW(g_path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    // Store as UTF-8 for easy sharing in bug reports.
    char utf8[2400];
    const int bytes = ::WideCharToMultiByte(CP_UTF8, 0, line, -1, utf8, sizeof(utf8), nullptr, nullptr);
    if (bytes > 1) {
        DWORD written = 0;
        ::WriteFile(file, utf8, static_cast<DWORD>(bytes - 1), &written, nullptr);
    }
    ::CloseHandle(file);
}

}  // namespace snippin::log
