#include "TestHarness.h"
#include "logic/Geometry.h"

using namespace snippin::logic;

TEST_CASE("NormalizeRect orders arbitrary corners") {
    CHECK((NormalizeRect(10, 20, 0, 5) == Rect{0, 5, 10, 20}));
    CHECK((NormalizeRect(0, 0, 3, 4) == Rect{0, 0, 3, 4}));
}

TEST_CASE("ClampRect intersects and returns empty when disjoint") {
    CHECK((ClampRect(Rect{-5, -5, 50, 50}, Rect{0, 0, 40, 30}) == Rect{0, 0, 40, 30}));
    CHECK(ClampRect(Rect{100, 100, 120, 120}, Rect{0, 0, 40, 30}).IsEmpty());
}

TEST_CASE("ScaleRegionToSurface is identity when sizes match") {
    const Rect r{10, 20, 110, 220};
    CHECK((ScaleRegionToSurface(r, Size{800, 600}, Size{800, 600}) == r));
}

TEST_CASE("ScaleRegionToSurface handles DPI-virtualised windows") {
    // A DPI-unaware window shown at 150 %: 1200x900 physical, 800x600 surface.
    const Rect physical{150, 300, 450, 600};
    const Rect surface = ScaleRegionToSurface(physical, Size{1200, 900}, Size{800, 600});
    CHECK((surface == Rect{100, 200, 300, 400}));
}

TEST_CASE("ScaleRegionToSurface clamps to the surface") {
    const Rect surface = ScaleRegionToSurface(Rect{0, 0, 2000, 2000}, Size{1000, 1000}, Size{500, 500});
    CHECK((surface == Rect{0, 0, 500, 500}));
}

TEST_CASE("ZoomSize scales and clamps") {
    CHECK((ZoomSize(Size{200, 100}, 1.0) == Size{200, 100}));
    CHECK((ZoomSize(Size{200, 100}, 2.0) == Size{400, 200}));
    CHECK((ZoomSize(Size{200, 100}, 100.0) == Size{800, 400}));  // clamped to 4x
    CHECK((ZoomSize(Size{1, 1}, 0.01) == Size{1, 1}));            // never below 1px
}

TEST_CASE("KeepAspect follows the dragged edge") {
    CHECK((KeepAspect(Size{400, 200}, Size{800, 999}, true) == Size{800, 400}));
    CHECK((KeepAspect(Size{400, 200}, Size{999, 100}, false) == Size{200, 100}));
}

TEST_CASE("Opacity stepping clamps between minimum and 100") {
    CHECK(StepOpacityPercent(100, 10, 20) == 100);
    CHECK(StepOpacityPercent(100, -10, 20) == 90);
    CHECK(StepOpacityPercent(25, -10, 20) == 20);
    CHECK(ClampOpacityPercent(0, 0) == 1);  // a window can never become fully invisible
}

TEST_CASE("Percent and alpha conversions round-trip") {
    CHECK(PercentToAlpha(100) == 255);
    CHECK(PercentToAlpha(0) == 0);
    for (int p = 0; p <= 100; ++p) {
        CHECK(AlphaToPercent(PercentToAlpha(p)) == p);
    }
}

TEST_CASE("FitInside keeps windows on screen") {
    const Rect work{0, 0, 1920, 1040};
    CHECK((FitInside(Rect{1900, 1000, 2100, 1100}, work) == Rect{1720, 940, 1920, 1040}));
    CHECK((FitInside(Rect{-50, -20, 150, 80}, work) == Rect{0, 0, 200, 100}));
    CHECK((FitInside(Rect{10, 10, 20, 20}, work) == Rect{10, 10, 20, 20}));
}
