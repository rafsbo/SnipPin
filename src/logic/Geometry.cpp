#include "logic/Geometry.h"

#include <algorithm>
#include <cmath>

namespace snippin::logic {

Rect NormalizeRect(int x1, int y1, int x2, int y2) {
    return Rect{std::min(x1, x2), std::min(y1, y2), std::max(x1, x2), std::max(y1, y2)};
}

Rect ClampRect(const Rect& r, const Rect& bounds) {
    Rect out{std::max(r.left, bounds.left), std::max(r.top, bounds.top),
             std::min(r.right, bounds.right), std::min(r.bottom, bounds.bottom)};
    if (out.IsEmpty()) {
        return Rect{};
    }
    return out;
}

Rect OffsetRect(const Rect& r, int dx, int dy) {
    return Rect{r.left + dx, r.top + dy, r.right + dx, r.bottom + dy};
}

Rect ScaleRegionToSurface(const Rect& regionInWindow, Size windowSize, Size surfaceSize) {
    if (windowSize.cx <= 0 || windowSize.cy <= 0 || surfaceSize.cx <= 0 || surfaceSize.cy <= 0 ||
        windowSize == surfaceSize) {
        return regionInWindow;
    }
    const double sx = static_cast<double>(surfaceSize.cx) / windowSize.cx;
    const double sy = static_cast<double>(surfaceSize.cy) / windowSize.cy;
    Rect out{static_cast<int>(std::floor(regionInWindow.left * sx)),
             static_cast<int>(std::floor(regionInWindow.top * sy)),
             static_cast<int>(std::ceil(regionInWindow.right * sx)),
             static_cast<int>(std::ceil(regionInWindow.bottom * sy))};
    return ClampRect(out, Rect{0, 0, surfaceSize.cx, surfaceSize.cy});
}

Size ZoomSize(Size base, double zoom) {
    const double z = ClampZoom(zoom);
    return Size{std::max(1, static_cast<int>(std::lround(base.cx * z))),
                std::max(1, static_cast<int>(std::lround(base.cy * z)))};
}

Size KeepAspect(Size base, Size requested, bool widthDriven) {
    if (base.cx <= 0 || base.cy <= 0) {
        return requested;
    }
    if (widthDriven) {
        const int w = std::max(1, requested.cx);
        return Size{w, std::max(1, static_cast<int>(std::lround(
                                       static_cast<double>(w) * base.cy / base.cx)))};
    }
    const int h = std::max(1, requested.cy);
    return Size{std::max(1, static_cast<int>(std::lround(static_cast<double>(h) * base.cx / base.cy))),
                h};
}

double ClampZoom(double zoom) {
    if (!(zoom == zoom)) {  // NaN guard
        return 1.0;
    }
    return std::clamp(zoom, kMinZoom, kMaxZoom);
}

int ClampOpacityPercent(int percent, int minPercent) {
    const int lo = std::clamp(minPercent, 1, 100);
    return std::clamp(percent, lo, 100);
}

int StepOpacityPercent(int current, int deltaPercent, int minPercent) {
    return ClampOpacityPercent(current + deltaPercent, minPercent);
}

std::uint8_t PercentToAlpha(int percent) {
    const int p = std::clamp(percent, 0, 100);
    return static_cast<std::uint8_t>((p * 255 + 50) / 100);
}

int AlphaToPercent(std::uint8_t alpha) {
    return (static_cast<int>(alpha) * 100 + 127) / 255;
}

Rect FitInside(const Rect& window, const Rect& workArea) {
    Rect r = window;
    if (r.right > workArea.right) {
        r = OffsetRect(r, workArea.right - r.right, 0);
    }
    if (r.bottom > workArea.bottom) {
        r = OffsetRect(r, 0, workArea.bottom - r.bottom);
    }
    if (r.left < workArea.left) {
        r = OffsetRect(r, workArea.left - r.left, 0);
    }
    if (r.top < workArea.top) {
        r = OffsetRect(r, 0, workArea.top - r.top);
    }
    return r;
}

}  // namespace snippin::logic
