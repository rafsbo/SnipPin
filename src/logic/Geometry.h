// Pure geometry helpers. This header must not include <windows.h> so the
// logic can be unit-tested on any platform.
#pragma once

#include <cstdint>

namespace snippin::logic {

struct Rect {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;

    constexpr int Width() const { return right - left; }
    constexpr int Height() const { return bottom - top; }
    constexpr bool IsEmpty() const { return right <= left || bottom <= top; }
    constexpr bool Contains(int x, int y) const {
        return x >= left && x < right && y >= top && y < bottom;
    }
    constexpr bool operator==(const Rect& o) const {
        return left == o.left && top == o.top && right == o.right && bottom == o.bottom;
    }
    constexpr bool operator!=(const Rect& o) const { return !(*this == o); }
};

struct Size {
    int cx = 0;
    int cy = 0;
    constexpr bool operator==(const Size& o) const { return cx == o.cx && cy == o.cy; }
};

// Builds a well-formed rectangle from two arbitrary corner points.
Rect NormalizeRect(int x1, int y1, int x2, int y2);

// Intersects |r| with |bounds|. Returns an empty rect when they do not overlap.
Rect ClampRect(const Rect& r, const Rect& bounds);

// Moves |r| by (dx, dy).
Rect OffsetRect(const Rect& r, int dx, int dy);

// Converts a region expressed in physical window pixels into the coordinate
// space of a DWM thumbnail / PrintWindow surface. The surface can be smaller
// than the physical window when Windows bitmap-stretches DPI-unaware apps.
Rect ScaleRegionToSurface(const Rect& regionInWindow, Size windowSize, Size surfaceSize);

// Scales a size by a zoom factor (1.0 = 100 %), never returning less than 1x1.
Size ZoomSize(Size base, double zoom);

// Keeps |base| aspect ratio while following a user resize. When
// |widthDriven| is true the new width wins, otherwise the new height wins.
Size KeepAspect(Size base, Size requested, bool widthDriven);

// Clamps a zoom factor to the supported range.
double ClampZoom(double zoom);
constexpr double kMinZoom = 0.25;
constexpr double kMaxZoom = 4.0;

// Opacity helpers. Percent values are always within [minPercent, 100].
int StepOpacityPercent(int current, int deltaPercent, int minPercent);
int ClampOpacityPercent(int percent, int minPercent);
std::uint8_t PercentToAlpha(int percent);
int AlphaToPercent(std::uint8_t alpha);

// Keeps a window of size |size| at |pos| fully inside |workArea| when possible.
Rect FitInside(const Rect& window, const Rect& workArea);

}  // namespace snippin::logic
