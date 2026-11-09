RaceMenu Atelier 1.2.0
======================

A native interface for RaceMenu, built on SKSE Menu Framework.
RaceMenu is not modified: its own menu stays loaded underneath and does the actual work.

Requirements
------------
- SKSE64 for your game version
- Address Library for SKSE Plugins
- RaceMenu
- SKSE Menu Framework 3.x (and its requirements)

Install
-------
Install with Mod Organizer 2 or Vortex and enable it. No ESP, nothing to sort.
Open RaceMenu as usual (new game, showracemenu, a mirror mod...).

Controls
--------
F4              switch between Atelier and RaceMenu's own interface
Drag / wheel    rotate the character / zoom between face and full body
Q / E           turn left / right
Z               face close-up / whole body
L               RaceMenu light on / off
R               finish character creation
Ctrl+Z / Ctrl+Y undo / redo
Ctrl+F          search every slider

BodySlide
---------
Click a value to type it. CBBE 3BA, HIMBO and UBE body morphs accept typed values
past the slider range, the same way BodySlide does (slider 0..100, type -150 or 160).
The BodySlide section of the side panel exports the current body morphs as a
BodySlide slider preset (CBBE 3BA, HIMBO or UBE) and imports any BodySlide slider
preset onto the character's RaceMenu sliders. Small/big values are blended by the
character's weight; import is one undo step. Build the body in BodySlide with zeroed
sliders, as for OBody.

FaceGen for NPC
---------------
Tools > FaceGen for NPC writes the face of your character as the pre-built head
(FaceGen) of any NPC you pick from a list, plus a JSON with the face's record data.
No Creation Kit needed. Two xEdit scripts (docs\RaceMenu Atelier - xEdit Scripts,
copy all three .pas files into xEdit's "Edit Scripts" folder) finish the job:
- Atelier - Apply NPC Appearance: writes head parts, hair colour, skin, tint layers,
  face sliders, height and weight into the NPC record.
- Atelier - Build NPC Mod: builds a self-contained mod - copies the head parts and
  every file they need, writes the face, copies the FaceGen, writes a manifest.
Full guide: docs\RaceMenu Atelier - FaceGen for NPC Guide (EN).md (Russian: (RU).md).
xEdit 4.1.5 or newer.

Settings
--------
SKSE\Plugins\RaceMenuAtelier.ini - most options can also be changed in game (Options button).

Uninstall
---------
Disable or remove the mod. RaceMenu returns to its stock interface; saves are not affected.

Source code (GPL-3.0): https://github.com/rustyoldsword/RaceMenuAtelier
