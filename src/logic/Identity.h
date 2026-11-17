// Window identity matching used by the stash/history. HWND values do not
// survive restarts, so windows are recognised by process path, window class,
// title and (for Explorer) folder path.
#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace snippin::logic {

struct WindowIdentity {
    std::wstring exePath;     // full path of the owning process image
    std::wstring className;   // window class name
    std::wstring title;       // window caption at the time it was recorded
    std::wstring folderPath;  // Explorer windows only, empty otherwise
};

// Case-insensitive equality (ASCII + Latin-1 aware via towlower).
bool EqualsNoCase(const std::wstring& a, const std::wstring& b);

// Score in [0, 100] describing how well |candidate| matches |entry|.
//   0   -> different application or class (never a match)
//   100 -> same folder (Explorer) or identical title
//   70  -> one title contains the other
//   40  -> same application/class but different title
int MatchScore(const WindowIdentity& entry, const WindowIdentity& candidate);

// True when two identities describe the "same" stash entry (dedup key).
bool SameStashKey(const WindowIdentity& a, const WindowIdentity& b);

// Moves |item| to the front of a most-recently-used list, replacing an
// existing element that |sameKey| considers equal, and trims to |maxItems|.
template <typename T, typename SameKey>
void PushMru(std::vector<T>& list, T item, std::size_t maxItems, SameKey sameKey) {
    list.erase(std::remove_if(list.begin(), list.end(),
                              [&](const T& existing) { return sameKey(existing, item); }),
               list.end());
    list.insert(list.begin(), std::move(item));
    if (list.size() > maxItems) {
        list.resize(maxItems);
    }
}

}  // namespace snippin::logic
