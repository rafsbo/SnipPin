#include "logic/Identity.h"

#include <cwctype>

namespace snippin::logic {
namespace {

std::wstring Lower(const std::wstring& s) {
    std::wstring out(s);
    for (auto& c : out) {
        c = static_cast<wchar_t>(std::towlower(c));
    }
    return out;
}

}  // namespace

bool EqualsNoCase(const std::wstring& a, const std::wstring& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::towlower(a[i]) != std::towlower(b[i])) {
            return false;
        }
    }
    return true;
}

int MatchScore(const WindowIdentity& entry, const WindowIdentity& candidate) {
    if (!EqualsNoCase(entry.exePath, candidate.exePath) ||
        !EqualsNoCase(entry.className, candidate.className)) {
        return 0;
    }
    if (!entry.folderPath.empty()) {
        // Explorer windows are only interchangeable when showing the same folder.
        return EqualsNoCase(entry.folderPath, candidate.folderPath) ? 100 : 0;
    }
    if (entry.title == candidate.title) {
        return 100;
    }
    const std::wstring a = Lower(entry.title);
    const std::wstring b = Lower(candidate.title);
    if (!a.empty() && !b.empty() &&
        (a.find(b) != std::wstring::npos || b.find(a) != std::wstring::npos)) {
        return 70;
    }
    return 40;
}

bool SameStashKey(const WindowIdentity& a, const WindowIdentity& b) {
    if (!EqualsNoCase(a.exePath, b.exePath) || !EqualsNoCase(a.className, b.className)) {
        return false;
    }
    if (!a.folderPath.empty() || !b.folderPath.empty()) {
        return EqualsNoCase(a.folderPath, b.folderPath);
    }
    return a.title == b.title;
}

}  // namespace snippin::logic
