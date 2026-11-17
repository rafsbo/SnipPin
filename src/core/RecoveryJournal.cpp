#include "core/RecoveryJournal.h"

#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <vector>

#include "platform/Log.h"
#include "platform/WinUtil.h"

namespace snippin {

void RecoveryJournal::Initialize(const std::wstring& path) { ini_ = IniFile(path); }

std::wstring RecoveryJournal::SectionName(HWND hwnd) {
    wchar_t name[32];
    _snwprintf_s(name, _countof(name), _TRUNCATE, L"W%llX",
                 static_cast<unsigned long long>(reinterpret_cast<ULONG_PTR>(hwnd)));
    return name;
}

void RecoveryJournal::Record(const JournalEntry& e) {
    const std::wstring sec = SectionName(e.hwnd);
    ini_.SetString(sec.c_str(), L"Hwnd", std::to_wstring(reinterpret_cast<ULONG_PTR>(e.hwnd)));
    ini_.SetString(sec.c_str(), L"Pid", std::to_wstring(e.pid));
    ini_.SetString(sec.c_str(), L"ExStyle", std::to_wstring(static_cast<long long>(e.originalExStyle)));
    ini_.SetBool(sec.c_str(), L"Topmost", e.originallyTopmost);
    ini_.SetBool(sec.c_str(), L"TopmostTouched", e.topmostTouched);
    ini_.SetBool(sec.c_str(), L"LayeredAttrs", e.layeredAttrsValid);
    ini_.SetInt(sec.c_str(), L"Alpha", e.originalAlpha);
    ini_.SetString(sec.c_str(), L"LwaFlags", std::to_wstring(e.originalLwaFlags));
    ini_.SetString(sec.c_str(), L"ColorKey", std::to_wstring(e.originalColorKey));
    ini_.Flush();
}

void RecoveryJournal::Remove(HWND hwnd) {
    ini_.DeleteSection(SectionName(hwnd).c_str());
    ini_.Flush();
}

void RecoveryJournal::Clear() {
    ::DeleteFileW(ini_.Path().c_str());
}

void RecoveryJournal::RestoreWindow(const JournalEntry& e) {
    if (!::IsWindow(e.hwnd)) {
        return;
    }
    const LONG_PTR managedBits = WS_EX_LAYERED | WS_EX_TRANSPARENT;
    const LONG_PTR current = ::GetWindowLongPtrW(e.hwnd, GWL_EXSTYLE);
    const LONG_PTR restored = (current & ~managedBits) | (e.originalExStyle & managedBits);
    if (restored != current) {
        ::SetWindowLongPtrW(e.hwnd, GWL_EXSTYLE, restored);
    }
    if ((e.originalExStyle & WS_EX_LAYERED) && e.layeredAttrsValid) {
        ::SetLayeredWindowAttributes(e.hwnd, e.originalColorKey, e.originalAlpha, e.originalLwaFlags);
    }
    if (e.topmostTouched) {
        ::SetWindowPos(e.hwnd, e.originallyTopmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    }
    ::RedrawWindow(e.hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN | RDW_ERASE);
}

int RecoveryJournal::RestoreOrphans() {
    // Enumerate section names (double-NUL terminated list).
    std::vector<wchar_t> names(32768);
    const DWORD len = ::GetPrivateProfileSectionNamesW(names.data(), static_cast<DWORD>(names.size()),
                                                       ini_.Path().c_str());
    int restored = 0;
    for (const wchar_t* p = names.data(); p < names.data() + len && *p; p += wcslen(p) + 1) {
        JournalEntry e;
        e.hwnd = reinterpret_cast<HWND>(
            static_cast<ULONG_PTR>(std::wcstoull(ini_.GetString(p, L"Hwnd").c_str(), nullptr, 10)));
        e.pid = static_cast<DWORD>(std::wcstoul(ini_.GetString(p, L"Pid").c_str(), nullptr, 10));
        e.originalExStyle = static_cast<LONG_PTR>(std::wcstoll(ini_.GetString(p, L"ExStyle").c_str(), nullptr, 10));
        e.originallyTopmost = ini_.GetBool(p, L"Topmost", false);
        e.topmostTouched = ini_.GetBool(p, L"TopmostTouched", false);
        e.layeredAttrsValid = ini_.GetBool(p, L"LayeredAttrs", false);
        e.originalAlpha = static_cast<BYTE>(ini_.GetInt(p, L"Alpha", 255));
        e.originalLwaFlags = static_cast<DWORD>(std::wcstoul(ini_.GetString(p, L"LwaFlags").c_str(), nullptr, 10));
        e.originalColorKey = static_cast<COLORREF>(std::wcstoul(ini_.GetString(p, L"ColorKey").c_str(), nullptr, 10));
        // HWND values are recycled: only restore if the same process owns it.
        if (e.hwnd && ::IsWindow(e.hwnd) && win::GetWindowProcessId(e.hwnd) == e.pid) {
            RestoreWindow(e);
            ++restored;
        }
    }
    Clear();
    if (restored) {
        SNIPPIN_LOG(L"Recovery journal restored %d window(s) from a previous session", restored);
    }
    return restored;
}

}  // namespace snippin
