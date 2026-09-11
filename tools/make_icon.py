#!/usr/bin/env python3
"""Generate mdit's application icon (mirrors the Pengy mark).

Design (mirrors ~/Personal/Pengy/pengy.png, the mascot icon):
  * **solid white disc**, fully transparent outside the circle — the same
    geometry as the Pengy icon (radius = 0.4872 * size, i.e. a 1.28% transparent
    margin on each side, verified against the source PNG);
  * instead of the penguin, a bold **`#`** (the markdown ATX heading element) in
    the Pengy near-black (#151515, sampled from the mascot's body) — drawn as
    four rounded bars, so the mark is crisp and font-independent at 16 px.

Writes:
  assets/icons/mdit-<size>.png   16 … 512 px, one natively-rendered file per size
  assets/mdit.svg                scalable master (same geometry, no font needed)

Re-run this whenever the artwork changes:  python3 tools/make_icon.py
(Needs Pillow — a *generation-time* tool only; the build itself needs nothing
but the committed PNG/`qrc` files.)
"""

from __future__ import annotations

import os
import sys

try:
    from PIL import Image, ImageDraw
except ImportError:  # pragma: no cover - generation-time tool
    sys.exit("Pillow is required to (re)generate the icon: python3 -m pip install pillow")

# --- Geometry (fractions of the icon size) -----------------------------------
DISC_RADIUS = 0.4872  # matches pengy.png: 573/1176 = 0.48724
HASH_HEIGHT = 0.54  # ink height of the '#'
HASH_WIDTH = 0.56  # ink width of the '#'
BAR_THICK = 0.19 * HASH_HEIGHT  # stroke weight, relative to the ink height
VERT_X = 0.20 * HASH_WIDTH  # vertical bars sit at (+/-) this x
HORZ_Y = 0.25 * HASH_HEIGHT  # horizontal bars sit at (+/-) this y
BAR_ROUND = 0.28 * BAR_THICK  # softly rounded bar ends (cartoon-friendly)

# In small sizes a sub-pixel (antialiased) 4-bar '#' turns to grey mush: the
# 16 px icon is drawn as a pixel-snapped, square-ended hash instead (see
# render_snapped), and the 24-32 px sizes get a slightly LIGHTER stroke with
# wider counter-spaces so the white between the bars survives. Everything
# >= 48 px uses the true weight and the soft rounded ends.
SNAPPED_MAX = 20
SMALL_MAX = 32
SMALL_THICK = 0.175
SMALL_VERT_X = 0.22
SMALL_HORZ_Y = 0.27

DISC_COLOR = (255, 255, 255, 255)
HASH_COLOR = (0x15, 0x15, 0x15, 255)

SIZES = (16, 24, 32, 48, 64, 128, 256, 512)


def _bar_rects(
    width: float,
    height: float,
    thick_factor: float = BAR_THICK,
    vert_x: float = VERT_X,
    horz_y: float = HORZ_Y,
) -> list[tuple[float, float, float, float]]:
    """The four bars of the '#', as (x0, y0, x1, y1) boxes around the centre."""
    th = thick_factor * height
    rects = []
    for sign in (-1.0, 1.0):  # the two verticals span the full ink height
        x = sign * vert_x * width
        rects.append((x - th / 2, -height / 2, x + th / 2, height / 2))
    for sign in (-1.0, 1.0):  # the two horizontals span the full ink width
        y = sign * horz_y * height
        rects.append((-width / 2, y - th / 2, width / 2, y + th / 2))
    return rects


def _thick_factor(size: int) -> float:
    """Stroke weight for `size` (lighter strokes at 24-32 px, see SMALL_THICK)."""
    return SMALL_THICK if size <= SMALL_MAX else BAR_THICK


def _bar_layout(size: int) -> tuple[float, float]:
    """The (vertical, horizontal) bar offsets for `size` (opened up when small)."""
    if size <= SMALL_MAX:
        return SMALL_VERT_X, SMALL_HORZ_Y
    return VERT_X, HORZ_Y


def render_snapped(size: int) -> Image.Image:
    """The 16 px variant: a whole-pixel, square-ended '#' on a smooth disc.

    Antialiased halftone bars read as a grey blob at this scale; snapping every
    bar to whole pixels (2 px thick, 3 px of white between them) keeps the mark
    legible the way hand-tuned 16x16 icons do it.
    """
    ss = 8
    big = Image.new("RGBA", (size * ss, size * ss), (0, 0, 0, 0))
    bd = ImageDraw.Draw(big)
    c = size * ss / 2.0
    r = DISC_RADIUS * size * ss
    bd.ellipse((c - r, c - r, c + r, c + r), fill=DISC_COLOR)
    img = big.reduce(ss)

    draw = ImageDraw.Draw(img)
    ink = max(7, round(0.56 * size))  # ink box, in whole pixels
    off = (size - ink) // 2
    t = max(2, round(0.22 * ink))  # bar thickness, in whole pixels
    near, far = off + 1, off + ink - 1 - t  # symmetric bar edges
    span0, span1 = off, off + ink - 1
    draw.rectangle((near, span0, near + t - 1, span1), fill=HASH_COLOR)
    draw.rectangle((far, span0, far + t - 1, span1), fill=HASH_COLOR)
    draw.rectangle((span0, near, span1, near + t - 1), fill=HASH_COLOR)
    draw.rectangle((span0, far, span1, far + t - 1), fill=HASH_COLOR)
    return img


def render(size: int, supersample: int) -> Image.Image:
    """Render the icon at `size` px (drawn at `supersample`x and box-filtered)."""
    if size <= SNAPPED_MAX:
        return render_snapped(size)
    s = size * supersample
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))  # transparent outside the disc
    draw = ImageDraw.Draw(img)

    c = s / 2.0
    r = DISC_RADIUS * s
    draw.ellipse((c - r, c - r, c + r, c + r), fill=DISC_COLOR)

    w = HASH_WIDTH * s
    h = HASH_HEIGHT * s
    vert_x, horz_y = _bar_layout(size)
    radius = max(1.0, BAR_ROUND * s)
    for x0, y0, x1, y1 in _bar_rects(w, h, _thick_factor(size), vert_x, horz_y):
        draw.rounded_rectangle(
            (c + x0, c + y0, c + x1, c + y1), radius=radius, fill=HASH_COLOR
        )

    # A box filter (reduce) rather than a windowed resample: at these ratios they
    # are equally smooth, but LANCZOS "rings" — it left a faint halo a pixel or
    # two OUTSIDE the disc edge, which then fails a strict transparency check
    # (and would show as a grey fringe on a dark desktop).
    return img.reduce(supersample)


def svg(view: int = 512) -> str:
    """The scalable master — the same disc + four bars, no font involved."""
    c = view / 2.0
    r = DISC_RADIUS * view
    w = HASH_WIDTH * view
    h = HASH_HEIGHT * view
    radius = BAR_ROUND * view
    bars = "\n".join(
        '    <rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h:.2f}"'
        ' rx="{r:.2f}" ry="{r:.2f}"/>'.format(
            x=c + x0, y=c + y0, w=x1 - x0, h=y1 - y0, r=radius
        )
        for x0, y0, x1, y1 in _bar_rects(w, h)
    )
    return (
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{view}" height="{view}"\n'
        f'     viewBox="0 0 {view} {view}">\n'
        f'  <title>mdit</title>\n'
        f'  <circle cx="{c:.2f}" cy="{c:.2f}" r="{r:.2f}" fill="#ffffff"/>\n'
        f'  <g fill="#151515">\n{bars}\n  </g>\n</svg>\n'
    )


def main() -> int:
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    icons = os.path.join(root, "assets", "icons")
    os.makedirs(icons, exist_ok=True)

    for size in SIZES:
        # Supersample more aggressively for the small sizes, where a 4-bar '#'
        # is the most demanding (each bar is only ~1.5 px wide at 16 px).
        supersample = 8 if size <= 64 else (4 if size <= 256 else 2)
        path = os.path.join(icons, f"mdit-{size}.png")
        render(size, supersample).save(path)
        print(f"wrote {os.path.relpath(path, root)} ({size}x{size})")

    svg_path = os.path.join(root, "assets", "mdit.svg")
    with open(svg_path, "w", encoding="utf-8") as fh:
        fh.write(svg())
    print(f"wrote {os.path.relpath(svg_path, root)} (scalable master)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
