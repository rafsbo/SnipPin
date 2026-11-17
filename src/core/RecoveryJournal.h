// Crash-safety journal. Every modification SnipPin makes to another
// application's window is written to journal.ini *before* it is applied, and
// removed once the window is restored. If SnipPin crashes or is killed, the
// next start restores any window that is still ghosted, translucent or
// topmost because of us.
#pragma once

#include <string>

#include "platform/Settings.h"
#include "platform/Win32.h"

namespace snippin {

struct JournalEntry {
    HWND hwnd = nullptr;
    DWORD pid = 0;
    LONG_PTR originalExStyle = 0;
    bool originallyTopmost = false;
    bool topmostTouched = false;
    bool layeredAttrsValid = false;
    BYTE originalAlpha = 255;
    DWORD originalLwaFlags = 0;
    COLORREF originalColorKey = 0;
};

class RecoveryJournal {
public:
    void Initialize(const std::wstring& path);

    void Record(const JournalEntry& entry);
    void Remove(HWND hwnd);
    void Clear();

    // Restores windows left modified by a previous run. Returns the count.
    int RestoreOrphans();

    // Shared restore routine (also used by WindowTracker).
    static void RestoreWindow(const JournalEntry& entry);

private:
    static std::wstring SectionName(HWND hwnd);
    IniFile ini_;
};

}  // namespace snippin
