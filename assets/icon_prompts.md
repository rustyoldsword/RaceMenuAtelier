# RaceMenu Atelier: icon prompts for ChatGPT

How to use:

1. Do everything in ONE ChatGPT chat, so the style stays the same.
2. First message: the STYLE block plus SHEET 1. Every next message:
   "Same style as the previous sheet, exactly: same white glyph look, same stroke weight, same size in the cell,
   transparent background." plus the next SHEET block.
3. Download every result as PNG, name it as in the sheet title (`sheet_categories_a.png` ...) and put it in
   `assets\sheets\`. I cut the sheets into icons myself.
4. Open each PNG before saving it. If the background is not really transparent (a grey-white checkerboard
   painted INTO the picture, a white or black fill), or a rule is broken (text, frames, circles behind the icon,
   colour, wrong count, icons touching), do not fix it yourself, just say:
   "Regenerate. Rules broken: <what>. The background must be real alpha transparency, not a drawn checkerboard."

Why white: the interface tints every icon itself (gold when active, white on hover, grey when idle, red for
danger), and a theme can recolour it. A white glyph takes any tint; a coloured picture would not.

---

## STYLE (paste before the first sheet)

```
You are drawing a sprite sheet of interface icons for the character creation menu of a premium dark-fantasy
RPG (think The Elder Scrolls, Baldur's Gate 3, Elden Ring menus). I will cut the sheet into separate icons
automatically, so the layout rules are strict.

BACKGROUND - MOST IMPORTANT RULE
- The background must be 100% TRANSPARENT: a real alpha channel, PNG with transparency.
- Do NOT paint a checkerboard pattern, a white, black, grey or coloured fill, a gradient, a texture,
  a vignette or any shadow on the background. Everything that is not an icon is fully transparent pixels.

CANVAS AND GRID
- Image size 1536 x 1024 pixels, landscape.
- An invisible grid of 3 columns x 2 rows, each cell exactly 512 x 512 pixels.
- Exactly one icon per cell, centred in its cell, the icon fits inside a 380 x 380 pixel square.
- At least 60 pixels of empty transparent space between an icon and its cell's edges. Nothing may cross into
  a neighbouring cell. No grid lines, no cell borders, no dividers.
- Cells are filled left to right, top row first. Cells I mark EMPTY must stay completely empty.

ICON STYLE (identical on every icon of every sheet)
- Pure WHITE glyphs (#FFFFFF) only. No other colour anywhere, no gold, no grey fill, no coloured shading.
  Inner detail is made with transparent cut-out lines inside the white shapes and with a second, softer
  level of white at about 55% opacity for secondary parts. Nothing else.
- Elegant engraved line-art, like icons stamped into a silver or gold plate: confident medium-thick strokes
  with slightly tapered ends, crisp corners, a tiny diamond or leaf accent at most once per icon.
  Luxurious and calm, not cartoonish, not childish, no sticker look, no gloss highlight.
- Stroke weight the same on every icon: about 7% of the icon's width. Strong and readable.
- Must stay readable when shrunk to 24 x 24 pixels: big clear silhouette, few details, no thin hairlines,
  no tiny decorations, no small text-like marks.
- Front view or clean three-quarter view, flat 2D, centred, upright, same visual weight across the sheet.

FORBIDDEN
- No text, letters, numbers, labels, captions, watermarks or signatures.
- No circles, rings, badges, shields, plates, frames, rounded squares or buttons behind or around the icons.
- No background objects, scenery, ground shadows, drop shadows, glow, outer glow, bevel lighting.
- No photorealism, no 3D render, no pixel art, no anime faces, no detailed human faces (a face is a simple
  profile or front silhouette), no extra icons beyond those listed.

Confirm nothing, just draw SHEET 1 below.
```

---

## SHEET 1: `sheet_categories_a.png` (slider categories, part 1)

```
SHEET: categories_a.

Cell 1 - RACE: three human head-and-shoulder silhouettes side by side, the middle one slightly larger and in
  front, the two behind it at 55% opacity.
Cell 2 - BODY: a standing human figure silhouette, front view, neutral pose, arms slightly away from the body,
  simple and elegant like a mannequin.
Cell 3 - HEAD: a head-and-shoulders bust in side profile facing right, like a classical cameo, no facial detail.
Cell 4 - FACE: a front-view face oval with a straight nose line and closed calm eyes drawn as two short curved
  lines, minimal, like a theatre mask without holes.
Cell 5 - EYES: one almond-shaped eye seen from the front, with an iris ring and a round pupil, and three
  short lashes on the upper lid.
Cell 6 - BROWS: two elegant arched eyebrows, one above each closed eye drawn as a single soft curve at 55%
  opacity.
```

## SHEET 2: `sheet_categories_b.png` (slider categories, part 2)

```
SHEET: categories_b.

Cell 1 - MOUTH: a pair of full lips seen from the front, closed, with the cupid's bow clearly shaped.
Cell 2 - HAIR: long flowing hair seen from behind, falling in three smooth wavy locks, no face visible.
Cell 3 - COLOURS: an artist's palette with a thumb hole and four round paint wells (the wells are transparent
  cut-outs), with a small brush resting across it.
Cell 4 - WAR PAINT: a vertical face outline (simple oval) with three bold diagonal war-paint stripes across
  one cheek, the outline at 55% opacity and the stripes in full white.
Cell 5 - BODY PAINT: a torso silhouette (shoulders to hips, front) with a flowing tribal swirl painted across
  it, the torso at 55% opacity and the swirl in full white.
Cell 6 - HAND PAINT: an open hand, palm facing the viewer, fingers together, with a henna-like ornament (a
  small flower and two curving lines) on the palm drawn as transparent cut-outs.
```

## SHEET 3: `sheet_categories_c.png` (slider categories, part 3)

```
SHEET: categories_c.

Cell 1 - FOOT PAINT: a bare foot seen from the side, with a delicate ornament band of small dots and a
  curving line across the top of the foot, drawn as transparent cut-outs.
Cell 2 - FACE PAINT: an elegant masquerade half-mask with two eye holes (transparent), pointed outer edges
  and one small diamond accent on the forehead.
Cell 3 - OVERLAYS: three stacked thin square sheets seen at a three-quarter angle, slightly fanned, the top
  one full white with a small ornamental swirl cut into it, the two below at 55% opacity.
Cell 4 - CUSTOM SLIDERS: three horizontal slider tracks stacked vertically, each with a diamond-shaped knob
  at a different position (left, right, middle).
Cell 5 - ALL CATEGORIES: a 3 x 3 grid of small rounded diamonds, evenly spaced, the centre one full white,
  the others at 55% opacity.
Cell 6 - EMPTY.
```

## SHEET 4: `sheet_toolbar.png` (editor toolbar)

```
SHEET: toolbar.

Cell 1 - UNDO: a bold curved arrow sweeping counter-clockwise from right to left, arrowhead on the left,
  tapered tail.
Cell 2 - REDO: the exact mirror image of cell 1, sweeping clockwise, arrowhead on the right.
Cell 3 - SEARCH: a magnifying glass, round lens with a thick rim and a short faceted handle pointing to the
  lower right, a small curved gleam inside the lens at 55% opacity.
Cell 4 - CHANGED ONLY: a funnel shape with a small four-point star sparkle at its upper right.
Cell 5 - SIDE PANEL: a tall rectangle window outline split into a wide left part and a narrow right column,
  the right column filled solid white.
Cell 6 - OPTIONS: a gear with eight rounded teeth and a round hole in the middle, with a thin inner ring at
  55% opacity.
```

## SHEET 5: `sheet_actions.png` (confirm, cancel, presets)

```
SHEET: actions.

Cell 1 - DONE: a bold elegant check mark with a calligraphic tapered stroke.
Cell 2 - CANCEL: a bold X made of two crossing tapered strokes.
Cell 3 - RESET: a circular arrow making almost a full circle counter-clockwise, arrowhead at the top left,
  with a small dot in the centre.
Cell 4 - SAVE PRESET: a closed leather-bound book seen from the front with a ribbon bookmark hanging from the
  bottom and a small diamond on the cover.
Cell 5 - LOAD PRESET: an open book seen from above, two pages spread, with a small arrow pointing up out of
  the pages.
Cell 6 - FOLDER: a closed document folder with a tab on the upper left, the front flap full white and the back
  part at 55% opacity.
```

## SHEET 6: `sheet_scene_a.png` (camera and light)

```
SHEET: scene_a.

Cell 1 - TURN LEFT: a circular arrow going around a small vertical axis line, arrowhead pointing left, like an
  orbit seen at a slight downward angle.
Cell 2 - TURN RIGHT: the exact mirror image of cell 1, arrowhead pointing right.
Cell 3 - CLOSE-UP ON FACE: a head-and-shoulders bust silhouette inside four corner brackets drawn close
  around the head (camera framing corners).
Cell 4 - WHOLE BODY: a small full standing figure silhouette inside four corner brackets drawn with more
  space around it.
Cell 5 - LIGHT ON: a radiant sun, round disc with eight straight tapered rays.
Cell 6 - LIGHT OFF: a crescent moon facing right with one small four-point star beside it.
```

## SHEET 7: `sheet_scene_b.png` (pose, clothing, sex)

```
SHEET: scene_b.

Cell 1 - UNDRESS: a simple tunic shirt with short sleeves, seen from the front, with a small arrow pointing
  up and away from its collar.
Cell 2 - FREEZE POSE: a six-armed snowflake, each arm with two short side branches, crisp and symmetric.
Cell 3 - PLAY POSE: a dancing human figure silhouette, one arm raised, one leg lifted, graceful.
Cell 4 - NEUTRAL STANCE: a standing human figure silhouette, front view, arms down by the sides, feet
  together, perfectly straight and still.
Cell 5 - MALE: the male symbol (a circle with an arrow pointing up and to the right), thick strokes, with
  one small diamond accent inside the circle at 55% opacity.
Cell 6 - FEMALE: the female symbol (a circle with a cross below it), thick strokes, with one small diamond
  accent inside the circle at 55% opacity.
```

## SHEET 8: `sheet_tools.png` (preset details, BodySlide, RaceMenu tools)

```
SHEET: tools.

Cell 1 - DETAILS: a small scroll with rolled ends and three short text lines drawn as plain bars (no letters)
  on it.
Cell 2 - BODYSLIDE EXPORT: a dressmaker's mannequin torso on a stand, with a small arrow pointing to the
  upper right out of it.
Cell 3 - RACEMENU INTERFACE: two crossed tools - a wrench and a fine artist's brush - forming an X.
Cell 4 - SCULPT: a sculptor's wire-loop modelling tool with a wooden-handle shape, its loop touching a small
  rounded lump of clay; a small four-point sparkle above the loop.
Cell 5 - DIAGNOSTIC REPORT: a clipboard with a small magnifying glass over its lower right corner, the
  clipboard at 55% opacity and the magnifier full white.
Cell 6 - READ AGAIN: two curved arrows chasing each other in a circle (refresh symbol), arrowheads at the top
  right and bottom left.
```

## SHEET 9 (optional): `sheet_ornaments.png` (header emblem and marker)

```
SHEET: ornaments. Same white glyph style, but these two may be a little more decorative.

Cell 1 - ATELIER EMBLEM: a heraldic-free monogram emblem for a character-creation workshop: a pair of
  tailor's scissors crossed with an artist's brush, inside a pointed diamond outline, symmetric. No letters.
Cell 2 - CHANGED MARKER: a small faceted gemstone seen from the front (cushion cut), facets drawn as
  transparent cut lines.
Cell 3 - EMPTY.
Cell 4 - EMPTY.
Cell 5 - EMPTY.
Cell 6 - EMPTY.
```

---

## Where each icon goes (for me)

| Sheet / cell | Icon file | Used for |
|---|---|---|
| categories_a 1-6 | cat_race, cat_body, cat_head, cat_face, cat_eyes, cat_brows | sidebar categories (flags 2, 4, 8, 16, 32, 64) |
| categories_b 1-6 | cat_mouth, cat_hair, cat_colors, cat_warpaint, cat_bodypaint, cat_handpaint | flags 128, 256, 8192, 16384, 32768, 65536 |
| categories_c 1-5 | cat_feetpaint, cat_facepaint, cat_overlays, cat_custom, cat_all | flags 131072, 262144, overlay/tattoo, other mods' categories, "All" |
| toolbar 1-6 | undo, redo, search, changed, sidepanel, options | toolbar, slider search, head-part browse |
| actions 1-6 | done, cancel, reset, save, load, folder | Done/confirm, modals, reset all, preset save/load, preset folders, Presets section |
| scene_a 1-6 | turn_left, turn_right, zoom_face, zoom_body, light_on, light_off | scene panel |
| scene_b 1-6 | undress, freeze, pose_play, pose_stop, male, female | scene panel, header sex switch, Scene section (pose_stop) |
| tools 1-6 | details, bodyslide, racemenu, sculpt, diagnostic, reread | preset info, BodySlide export button and section, RaceMenu tools section and banners |
| ornaments 1-2 | emblem, gem | header title, changed-slider marker |

Chevrons (list arrows, collapsible headers) stay Font Awesome glyphs: they are tiny and read best as plain
arrows.
