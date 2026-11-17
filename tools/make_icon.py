#!/usr/bin/env python3
"""Generates res/SnipPin.ico (multi-resolution) with Pillow.

Usage: python tools/make_icon.py
The design: a blue rounded tile with a dashed "snip" frame and a white pin.
"""
from pathlib import Path

from PIL import Image, ImageDraw

SIZES = [16, 20, 24, 32, 40, 48, 64, 128, 256]
ACCENT = (0, 120, 215, 255)
WHITE = (255, 255, 255, 255)
SCALE = 4  # supersampling factor for smooth edges


def draw(size: int) -> Image.Image:
    s = size * SCALE
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    pad = s * 0.04
    d.rounded_rectangle([pad, pad, s - pad, s - pad], radius=s * 0.2, fill=ACCENT)

    # Snip frame (corner brackets) in the lower-left area.
    w = max(SCALE, int(s * 0.07))
    x0, y0, x1, y1 = s * 0.18, s * 0.34, s * 0.68, s * 0.82
    arm = (x1 - x0) * 0.32
    for (cx, cy, dx, dy) in [(x0, y0, 1, 1), (x1, y0, -1, 1), (x0, y1, 1, -1), (x1, y1, -1, -1)]:
        d.line([(cx, cy), (cx + dx * arm, cy)], fill=WHITE, width=w)
        d.line([(cx, cy), (cx, cy + dy * arm)], fill=WHITE, width=w)

    # Pin: round head + needle, upper-right.
    hx, hy, r = s * 0.66, s * 0.32, s * 0.15
    d.ellipse([hx - r, hy - r, hx + r, hy + r], fill=WHITE)
    d.line([(hx - r * 0.3, hy + r * 0.3), (s * 0.43, s * 0.58)], fill=WHITE, width=max(SCALE, int(s * 0.06)))
    return img.resize((size, size), Image.LANCZOS)


def main() -> None:
    out = Path(__file__).resolve().parent.parent / "res" / "SnipPin.ico"
    images = [draw(sz) for sz in SIZES]
    images[-1].save(out, format="ICO", sizes=[(sz, sz) for sz in SIZES], append_images=images[:-1])
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
