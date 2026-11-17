// Minimal diagnostics: OutputDebugString (visible in DebugView) and an
// optional log file enabled with [Debug] LogToFile=1 in settings.ini.
#pragma once

#include <string>

namespace snippin::log {

void Initialize(const std::wstring& filePath, bool toFile);
void Write(const wchar_t* format, ...);

}  // namespace snippin::log

#define SNIPPIN_LOG(...) ::snippin::log::Write(__VA_ARGS__)
