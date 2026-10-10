"""Cut the icon sprite sheets (assets/icon_prompts.md) into the PNGs the DLL loads.

    python tools/slice_icons.py

assets/sheets/sheet_<name>.png are 1536 x 1024 sheets: a 3 x 2 grid of 512 px cells, read left to right, top row
first, white glyphs on a transparent background. Each icon is trimmed to its pixels, centred on a square with a
small margin and written at several sizes, so the interface can pick the one closest to the size it draws:

    dist_template/Interface/RaceMenuAtelier/icons/<icon>_<size>.png

Only alpha matters (the DLL tints the icons), so the colour is written as pure white and the alpha is resampled.
A preview of every icon on the panel colour goes to build/icons_preview.png.
"""
import os

import numpy as np
from PIL import Image
from scipy import ndimage

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHEETS = os.path.join(ROOT, "assets", "sheets")
OUT = os.path.join(ROOT, "dist_template", "Interface", "RaceMenuAtelier", "icons")
PREVIEW = os.path.join(ROOT, "build", "icons_preview.png")
SIZES = (24, 32, 48, 64, 96)
MARGIN = 0.04
CELL = 512

LAYOUT = {
    "categories_a": ["cat_race", "cat_body", "cat_head", "cat_face", "cat_eyes", "cat_brows"],
    "categories_b": ["cat_mouth", "cat_hair", "cat_colors", "cat_warpaint", "cat_bodypaint", "cat_handpaint"],
    "categories_c": ["cat_feetpaint", "cat_facepaint", "cat_overlays", "cat_custom", "cat_all"],
    "toolbar": ["undo", "redo", "search", "changed", "sidepanel", "options"],
    "actions": ["done", "cancel", "reset", "save", "load", "folder"],
    "scene_a": ["turn_left", "turn_right", "zoom_face", "zoom_body", "light_on", "light_off"],
    "scene_b": ["undress", "freeze", "pose_play", "pose_stop", "male", "female"],
    "tools": ["details", "bodyslide", "racemenu", "sculpt", "diagnostic", "reread"],
    "ornaments": ["emblem", "gem"],
}


def assign(alpha):
    """Label the ink of a whole sheet and give every shape to the cell holding its centre.

    Icons may poke a little into a neighbouring cell (a dancer's foot, an emblem's tip), so cutting on the grid
    lines would chop them and leave their tips as specks in the next cell."""
    alpha = np.where(alpha < 12, 0, alpha)
    ink = ndimage.binary_dilation(alpha > 0, iterations=2)
    labels, count = ndimage.label(ink)
    owner = np.zeros(count + 1, dtype=int) - 1
    for i, (cy, cx) in enumerate(ndimage.center_of_mass(ink, labels, range(1, count + 1)), start=1):
        owner[i] = int(cy // CELL) * 3 + int(cx // CELL)
    return alpha, labels, owner


def cut(alpha, labels, owner, index):
    ids = np.nonzero(owner == index)[0]
    if len(ids) == 0:
        return None
    sizes = ndimage.sum(np.ones_like(labels), labels, ids)
    # drop specks: shapes far smaller than the icon's main shape
    ids = [i for i, s in zip(ids, sizes) if s >= max(40, sizes.max() * 0.004)]
    mask = np.isin(labels, ids)
    ys, xs = np.nonzero(mask & (alpha > 0))
    if len(xs) == 0:
        return None
    crop = np.where(mask, alpha, 0)[ys.min():ys.max() + 1, xs.min():xs.max() + 1].astype(np.uint8)
    h, w = crop.shape
    side = int(round(max(w, h) * (1.0 + 2.0 * MARGIN)))
    square = np.zeros((side, side), dtype=np.uint8)
    y0, x0 = (side - h) // 2, (side - w) // 2
    square[y0:y0 + h, x0:x0 + w] = crop
    return Image.fromarray(square, "L")


def white(alpha_img):
    rgba = Image.new("RGBA", alpha_img.size, (255, 255, 255, 0))
    rgba.putalpha(alpha_img)
    return rgba


def main():
    os.makedirs(OUT, exist_ok=True)
    os.makedirs(os.path.dirname(PREVIEW), exist_ok=True)
    icons = []
    for sheet_name, names in LAYOUT.items():
        path = os.path.join(SHEETS, f"sheet_{sheet_name}.png")
        sheet = np.array(Image.open(path).convert("RGBA"))[..., 3]
        if sheet.shape != (2 * CELL, 3 * CELL):
            raise SystemExit(f"{path}: expected 1536 x 1024, got {sheet.shape[1]} x {sheet.shape[0]}")
        alpha_all, labels, owner = assign(sheet)
        for index, name in enumerate(names):
            alpha = cut(alpha_all, labels, owner, index)
            if alpha is None:
                raise SystemExit(f"{path}: cell {index + 1} ({name}) is empty")
            for size in SIZES:
                white(alpha.resize((size, size), Image.LANCZOS)).save(os.path.join(OUT, f"{name}_{size}.png"), optimize=True)
            icons.append((name, alpha))
    # preview: every icon at 96 and 24 px on the panel colour
    cols = 8
    rows = (len(icons) + cols - 1) // cols
    preview = Image.new("RGBA", (cols * 140, rows * 140), (30, 34, 42, 255))
    for i, (name, alpha) in enumerate(icons):
        x, y = (i % cols) * 140, (i // cols) * 140
        preview.alpha_composite(white(alpha.resize((96, 96), Image.LANCZOS)), (x + 8, y + 8))
        preview.alpha_composite(white(alpha.resize((24, 24), Image.LANCZOS)), (x + 108, y + 108))
    preview.convert("RGB").save(PREVIEW)
    print(f"{len(icons)} icons x {len(SIZES)} sizes -> {OUT}")
    print(f"preview -> {PREVIEW}")


if __name__ == "__main__":
    main()
