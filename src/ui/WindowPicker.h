// Crosshair window picker: a nearly invisible overlay covers the virtual
// screen, the window under the cursor is outlined, a click selects it.
#pragma once

#include <functional>

#include "platform/Win32.h"

namespace snippin {

class WindowPicker {
public:
    using Callback = std::function<void(HWND picked, POINT screenPt)>;

    WindowPicker() = default;
    ~WindowPicker();
    WindowPicker(const WindowPicker&) = delete;
    WindowPicker& operator=(const WindowPicker&) = delete;

    bool Begin(HINSTANCE instance, Callback onPicked);
    void Cancel();
    bool IsActive() const { return overlay_ != nullptr; }

private:
    static LRESULT CALLBACK OverlayProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK FrameProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleOverlay(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void UpdateHighlight(POINT screenPt);
    void Finish(bool accepted, POINT screenPt);
    static HWND WindowAt(POINT screenPt);

    HWND overlay_ = nullptr;
    HWND frame_ = nullptr;
    HWND hint_ = nullptr;
    HWND current_ = nullptr;
    Callback onPicked_;
};

}  // namespace snippin
