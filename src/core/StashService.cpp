#include "core/StashService.h"

#include <cstddef>
#include <cstdio>
#include <sstream>

#include "platform/WinUtil.h"

namespace snippin {
namespace {

constexpr size_t kMaxSnippetsPerEntry = 8;

std::uint64_t NowFileTime() {
    FILETIME ft;
    ::GetSystemTimeAsFileTime(&ft);
    return (static_cast<std::uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

std::wstring EncodeRegions(const std::vector<logic::Rect>& regions) {
    std::wstring out;
    for (const auto& r : regions) {
        if (!out.empty()) {
            out += L';';
        }
        out += std::to_wstring(r.left) + L',' + std::to_wstring(r.top) + L',' + std::to_wstring(r.right) +
               L',' + std::to_wstring(r.bottom);
    }
    return out;
}

std::vector<logic::Rect> DecodeRegions(const std::wstring& text) {
    std::vector<logic::Rect> out;
    std::wstringstream stream(text);
    std::wstring item;
    while (std::getline(stream, item, L';')) {
        logic::Rect r;
        if (swscanf_s(item.c_str(), L"%d,%d,%d,%d", &r.left, &r.top, &r.right, &r.bottom) == 4 && !r.IsEmpty()) {
            out.push_back(r);
        }
    }
    return out;
}

struct FindContext {
    const StashEntry* entry;
    HWND best;
    int bestScore;
};

BOOL CALLBACK FindProc(HWND hwnd, LPARAM lParam) {
    auto* ctx = reinterpret_cast<FindContext*>(lParam);
    if (!win::IsCandidateWindow(hwnd)) {
        return TRUE;
    }
    // Cheap pre-filter on class before querying the process path.
    if (!logic::EqualsNoCase(win::GetClassNameString(hwnd), ctx->entry->identity.className)) {
        return TRUE;
    }
    const int score = logic::MatchScore(ctx->entry->identity, win::GetIdentity(hwnd));
    if (score > ctx->bestScore) {
        ctx->best = hwnd;
        ctx->bestScore = score;
    }
    return ctx->bestScore < 100;  // stop at a perfect match
}

}  // namespace

void StashService::Initialize(const std::wstring& path, int maxEntries) {
    ini_ = IniFile(path);
    maxEntries_ = maxEntries;
    Load();
}

void StashService::SetMaxEntries(int maxEntries) {
    maxEntries_ = maxEntries;
    if (static_cast<int>(entries_.size()) > maxEntries_) {
        entries_.resize(static_cast<size_t>(maxEntries_));
        Changed();
    }
}

StashEntry* StashService::FindEntry(const logic::WindowIdentity& identity) {
    for (auto& e : entries_) {
        if (logic::SameStashKey(e.identity, identity)) {
            return &e;
        }
    }
    return nullptr;
}

void StashService::Remember(const logic::WindowIdentity& identity, bool topmost, int opacityPercent, bool ghost) {
    if (maxEntries_ <= 0 || identity.exePath.empty()) {
        return;
    }
    StashEntry entry;
    if (const StashEntry* existing = FindEntry(identity)) {
        entry = *existing;
    }
    entry.identity = identity;
    entry.topmost = topmost;
    entry.opacityPercent = opacityPercent;
    entry.ghost = ghost;
    entry.lastUsed = NowFileTime();
    logic::PushMru(entries_, std::move(entry), static_cast<size_t>(maxEntries_),
                   [](const StashEntry& a, const StashEntry& b) { return logic::SameStashKey(a.identity, b.identity); });
    Changed();
}

void StashService::RememberSnippet(const logic::WindowIdentity& identity, const logic::Rect& region) {
    if (maxEntries_ <= 0 || identity.exePath.empty()) {
        return;
    }
    StashEntry entry;
    entry.topmost = false;
    if (const StashEntry* existing = FindEntry(identity)) {
        entry = *existing;
    }
    entry.identity = identity;
    auto& regions = entry.snippetRegions;
    bool known = false;
    for (const auto& r : regions) {
        known = known || r == region;
    }
    if (!known) {
        regions.insert(regions.begin(), region);
        if (regions.size() > kMaxSnippetsPerEntry) {
            regions.resize(kMaxSnippetsPerEntry);
        }
    }
    entry.lastUsed = NowFileTime();
    logic::PushMru(entries_, std::move(entry), static_cast<size_t>(maxEntries_),
                   [](const StashEntry& a, const StashEntry& b) { return logic::SameStashKey(a.identity, b.identity); });
    Changed();
}

void StashService::Remove(size_t index) {
    if (index < entries_.size()) {
        entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(index));
        Changed();
    }
}

void StashService::Clear() {
    entries_.clear();
    Changed();
}

HWND StashService::FindLiveWindow(const StashEntry& entry) {
    FindContext ctx{&entry, nullptr, 0};
    ::EnumWindows(&FindProc, reinterpret_cast<LPARAM>(&ctx));
    return ctx.bestScore >= 70 ? ctx.best : nullptr;
}

bool StashService::Launch(const StashEntry& entry) {
    const std::wstring& target = entry.identity.folderPath.empty() ? entry.identity.exePath : entry.identity.folderPath;
    if (target.empty()) {
        return false;
    }
    const auto result = reinterpret_cast<INT_PTR>(
        ::ShellExecuteW(nullptr, L"open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    return result > 32;
}

void StashService::Changed() {
    Save();
    if (onChanged) {
        onChanged();
    }
}

void StashService::Load() {
    entries_.clear();
    const int count = ini_.GetInt(L"History", L"Count", 0);
    for (int i = 0; i < count && i < maxEntries_; ++i) {
        const std::wstring sec = L"Entry" + std::to_wstring(i);
        StashEntry e;
        e.identity.exePath = ini_.GetString(sec.c_str(), L"Exe");
        e.identity.className = ini_.GetString(sec.c_str(), L"Class");
        e.identity.title = ini_.GetString(sec.c_str(), L"Title");
        e.identity.folderPath = ini_.GetString(sec.c_str(), L"Folder");
        e.topmost = ini_.GetBool(sec.c_str(), L"Topmost", true);
        e.opacityPercent = ini_.GetInt(sec.c_str(), L"Opacity", 100);
        e.ghost = ini_.GetBool(sec.c_str(), L"Ghost", false);
        e.snippetRegions = DecodeRegions(ini_.GetString(sec.c_str(), L"Snippets"));
        e.lastUsed = std::wcstoull(ini_.GetString(sec.c_str(), L"LastUsed").c_str(), nullptr, 10);
        if (!e.identity.exePath.empty()) {
            entries_.push_back(std::move(e));
        }
    }
}

void StashService::Save() const {
    // Rewrite the whole file: simplest way to keep indices contiguous.
    ::DeleteFileW(ini_.Path().c_str());
    ini_.EnsureUnicode();
    ini_.SetInt(L"History", L"Count", static_cast<int>(entries_.size()));
    for (size_t i = 0; i < entries_.size(); ++i) {
        const auto& e = entries_[i];
        const std::wstring sec = L"Entry" + std::to_wstring(i);
        ini_.SetString(sec.c_str(), L"Exe", e.identity.exePath);
        ini_.SetString(sec.c_str(), L"Class", e.identity.className);
        ini_.SetString(sec.c_str(), L"Title", e.identity.title);
        ini_.SetString(sec.c_str(), L"Folder", e.identity.folderPath);
        ini_.SetBool(sec.c_str(), L"Topmost", e.topmost);
        ini_.SetInt(sec.c_str(), L"Opacity", e.opacityPercent);
        ini_.SetBool(sec.c_str(), L"Ghost", e.ghost);
        ini_.SetString(sec.c_str(), L"Snippets", EncodeRegions(e.snippetRegions));
        ini_.SetString(sec.c_str(), L"LastUsed", std::to_wstring(e.lastUsed));
    }
    ini_.Flush();
}

}  // namespace snippin
