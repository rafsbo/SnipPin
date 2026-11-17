// A floating, always-on-top widget that shows a region of another window.
//
// Renderers:
//   * Live (default): DWM thumbnail cropped to the region. GPU-composited,
//     near-zero CPU, keeps updating while the source is covered.
//   * Live fallback: PrintWindow polling, used when DWM composition is off
//     (Windows 7 Basic/Classic theme) or compatibility mode is forced.
//   * Snapshot: a frozen bitmap, handy to compare "before" and "after".
#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "logic/Geometry.h"
#include "platform/Win32.h"

namespace snippin {

struct SnippetOptions {
    bool live = true;
    bool compatibilityMode = false;
    int fallbackIntervalMs = 250;
    int opacityStepPercent = 10;
    int minOpacityPercent = 20;
};

class SnippetWindow {
public:
    struct Callbacks {
        std::function<void(SnippetWindow&, POINT screenPt)> onContextMenu;
        std::function<void(SnippetWindow&)> onCloseRequested;
        std::function<void(SnippetWindow&)> onChanged;
    };

    SnippetWindow(std::uint64_t id, Callbacks callbacks);
    ~SnippetWindow();
    SnippetWindow(const SnippetWindow&) = delete;
    SnippetWindow& operator=(const SnippetWindow&) = delete;

    bool Create(HINSTANCE instance, HWND source, const logic::Rect& regionInWindow,
                const logic::Rect& screenRegion, const SnippetOptions& options);
    void Destroy();

    std::uint64_t Id() const { return id_; }
    HWND Hwnd() const { return hwnd_; }
    HWND Source() const { return source_; }
    const std::wstring& SourceTitle() const { return sourceTitle_; }
    const logic::Rect& Region() const { return region_; }
    bool IsLive() const { return live_; }
    bool IsGhost() const { return ghost_; }
    int OpacityPercent() const { return opacityPercent_; }
    bool IsSourceClosed() const { return sourceClosed_; }
    std::uint64_t GhostSequence() const { return ghostSequence_; }

    void SetLive(bool live);
    void RefreshSnapshot();
    void SetOpacity(int percent, int minPercent);
    void SetGhost(bool ghost, int ghostOpacityPercent, int minPercent, std::uint64_t sequence);
    void SetZoom(double zoom);
    void OnWheel(int delta, bool ctrl, int opacityStep, int minPercent);
    void SetOptions(const SnippetOptions& options);
    void FocusSource();
    void ReassertTopmost();
    void OnSourceDestroyed();
    void OnCompositionChanged();

private:
    enum class Renderer { None, Thumbnail, PrintWindow, Frozen };

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void ChooseRenderer();
    void RegisterThumbnail();
    void UnregisterThumbnail();
    void UpdateThumbnail();
    bool CaptureWithPrintWindow();
    bool CaptureFromScreen(const logic::Rect& screenRegion);
    void ApplyLayeredStyle();
    void ResizeToZoom();
    void Paint(HDC dc);
    RECT ContentRect() const;
    int Border() const;
    LRESULT HitTest(POINT screenPt) const;
    void Changed();

    std::uint64_t id_;
    Callbacks callbacks_;
    HWND hwnd_ = nullptr;
    HWND source_ = nullptr;
    std::wstring sourceTitle_;
    logic::Rect region_;       // relative to the source window rect, physical px
    logic::Size baseSize_;     // region size at 100 % zoom
    double zoom_ = 1.0;
    int opacityPercent_ = 100;
    bool ghost_ = false;
    std::uint64_t ghostSequence_ = 0;
    bool live_ = true;
    bool sourceClosed_ = false;
    bool sourceMinimized_ = false;
    SnippetOptions options_;

    Renderer renderer_ = Renderer::None;
    HTHUMBNAIL thumbnail_ = nullptr;
    logic::Size lastSourceSize_;

    GdiObject<HBITMAP> frame_;  // captured pixels (PrintWindow or snapshot)
    logic::Rect frameCrop_;     // part of frame_ to display

    static constexpr UINT_PTR kSyncTimer = 1;
    static constexpr UINT kSyncIntervalMs = 500;
};

}  // namespace snippin
