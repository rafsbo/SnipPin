// Edge-docked tool window listing pinned windows, snippets and recent
// (stashed) windows. The panel only renders rows supplied by the App and
// reports activation / context-menu requests back to it.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "platform/Win32.h"

namespace snippin {

enum class PanelGroup { Pinned = 1, Snippets = 2, Recent = 3 };

struct PanelItemRef {
    PanelGroup group = PanelGroup::Pinned;
    HWND hwnd = nullptr;           // pinned window
    std::uint64_t snippetId = 0;   // snippet
    size_t recentIndex = 0;        // stash entry
};

struct PanelRow {
    PanelItemRef ref;
    std::wstring title;
    std::wstring state;
    HICON icon = nullptr;
    bool ownsIcon = false;  // destroy after adding to the image list
};

class StashPanel {
public:
    using RowProvider = std::function<std::vector<PanelRow>()>;
    using ItemHandler = std::function<void(const PanelItemRef&)>;
    using MenuHandler = std::function<void(const PanelItemRef&, POINT)>;

    ~StashPanel();

    bool Create(HINSTANCE instance, RowProvider rows, ItemHandler onActivate, MenuHandler onContextMenu);
    void Toggle();
    void Show();
    void Hide();
    bool IsVisible() const;
    void Refresh();
    void Destroy();
    HWND Hwnd() const { return hwnd_; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    void CreateList();
    void ApplyDpi(UINT dpi);
    void DockToCursorMonitor();
    void LayoutList();
    const PanelItemRef* RefAt(int index) const;

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND list_ = nullptr;
    HIMAGELIST images_ = nullptr;
    GdiObject<HFONT> font_;
    UINT dpi_ = 96;
    RowProvider rows_;
    ItemHandler onActivate_;
    MenuHandler onContextMenu_;
    std::vector<PanelItemRef> refs_;
};

}  // namespace snippin
