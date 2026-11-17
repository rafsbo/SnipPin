// Most-recently-used history of pinned windows ("stash"). Entries survive
// restarts (history.ini) and can be recalled: SnipPin finds the matching
// window again - or reopens the folder / relaunches the application - and
// re-applies always-on-top, opacity, ghost mode and snippet regions.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "logic/Geometry.h"
#include "logic/Identity.h"
#include "platform/Settings.h"
#include "platform/Win32.h"

namespace snippin {

struct StashEntry {
    logic::WindowIdentity identity;
    bool topmost = true;
    int opacityPercent = 100;
    bool ghost = false;
    std::vector<logic::Rect> snippetRegions;  // relative to the window rect
    std::uint64_t lastUsed = 0;               // FILETIME as integer
};

class StashService {
public:
    void Initialize(const std::wstring& path, int maxEntries);
    void SetMaxEntries(int maxEntries);

    const std::vector<StashEntry>& Entries() const { return entries_; }

    // Inserts or refreshes the entry for |identity| and moves it to the top.
    void Remember(const logic::WindowIdentity& identity, bool topmost, int opacityPercent, bool ghost);
    void RememberSnippet(const logic::WindowIdentity& identity, const logic::Rect& region);
    void Remove(size_t index);
    void Clear();

    // Best live window for an entry (score >= 70), or nullptr.
    static HWND FindLiveWindow(const StashEntry& entry);

    // Starts the application / opens the folder for an entry.
    static bool Launch(const StashEntry& entry);

    std::function<void()> onChanged;

private:
    StashEntry* FindEntry(const logic::WindowIdentity& identity);
    void Load();
    void Save() const;
    void Changed();

    IniFile ini_;
    std::vector<StashEntry> entries_;
    int maxEntries_ = 20;
};

}  // namespace snippin
