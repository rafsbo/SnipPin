// Full-window overlay used to draw the rectangle that becomes a snippet.
// Drag = custom region, single click = whole window, Esc / right-click = cancel.
#pragma once

#include <functional>

#include "logic/Geometry.h"
#include "platform/Win32.h"

namespace snippin {

class RegionSelector {
public:
    // |regionInWindow| is relative to the target's GetWindowRect origin, in
    // physical pixels. |screenRegion| is the same area in screen coordinates.
    using Callback = std::function<void(HWND target, logic::Rect regionInWindow, logic::Rect screenRegion)>;

    RegionSelector() = default;
    ~RegionSelector();
    RegionSelector(const RegionSelector&) = delete;
    RegionSelector& operator=(const RegionSelector&) = delete;

    bool Begin(HINSTANCE instance, HWND target, Callback onSelected);
    void Cancel();
    bool IsActive() const { return hwnd_ != nullptr; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    void Paint(HDC dc);
    void Finish(bool accepted);
    logic::Rect SelectionInClient() const;

    HWND hwnd_ = nullptr;
    HWND target_ = nullptr;
    RECT overlayScreen_{};  // visible frame of the target
    RECT targetWindow_{};   // GetWindowRect of the target
    POINT anchor_{};
    POINT current_{};
    bool dragging_ = false;
    bool finishing_ = false;
    Callback onSelected_;
};

}  // namespace snippin
