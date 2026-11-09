# RaceMenu Atelier

A native C++ interface for RaceMenu, drawn with Dear ImGui through
[SKSE Menu Framework](https://www.nexusmods.com/skyrimspecialedition/mods/120352).

RaceMenu itself is left untouched. Its Scaleform menu stays loaded underneath, hidden, and is used as
the data source and action target: Atelier reads the categories and sliders RaceMenu exposes and sends
every change back through RaceMenu's own delegate calls, mod events and `CharGen` functions. Press
**F4** in the editor to switch to RaceMenu's own interface and back at any time.

## Features

- One searchable editor for every slider, with a "changed only" filter, undo/redo and per-slider reset
- Typed values, optional overdrive past the registered range of custom body morphs
- Colour editor with palette and recent colours; texture browser for tints, paints and overlays
- Head part browser with search by name, form ID or plugin
- Scene panel: drag to rotate, wheel to zoom (face / full body), light toggle, undress / redress,
  pose freeze, a small set of idle poses
- Preset browser that shows which plugins, head parts and tint textures a `.jslot` uses before loading it
- BodySlide preset export (CBBE 3BA, HIMBO) straight from the current morphs
- FaceGen for NPC: writes the face made on the player as the FaceGen head of another NPC record
- No ESP, no Papyrus, no replaced RaceMenu files

## FaceGen for NPC

Tools > **FaceGen for NPC** takes an NPC and writes the player's current face as that NPC's
pre-built head, the way the Creation Kit's "Export FaceGen" (Ctrl+F4) would:

| File | Content |
|---|---|
| `meshes\actors\character\FaceGenData\FaceGeom\<plugin>\<id>.nif` | the head |
| `textures\actors\character\FaceGenData\FaceTint\<plugin>\<id>.dds` | the face tint |
| `SKSE\Plugins\RaceMenuAtelier\NPCFaces\<plugin>\<id>.json` | the record data for xEdit |

`<plugin>` is the plugin that defines the NPC and `<id>` its local FormID in eight hex digits
(a light plugin uses its 12-bit ID: `00000D62`). Under Mod Organizer 2 all three land in Overwrite.

Press **Choose the NPC...** to pick the target: a window lists every NPC record of the loaded
plugins, with a plugin filter (it has its own search box) and a search by name, EditorID or FormID
(local `D62` or the runtime `FE17ED62` the console prints). EditorIDs show only when an EditorID
cache such as powerofthree's Tweaks keeps them in memory. NPCs drawn in grey take their face from a
template, so the game would show the template's FaceGen instead. Pick a row, then press **Write**.

How the head is built:

- One shape per head part the NPC will expect: the player's head parts, the race defaults for any
  empty single slot (face, eyes, hair, facial hair, brows), the race's default Misc parts (the mouth)
  and all extra parts. Each shape is named by its head part's EditorID.
- Each shape comes from that head part's own model, so partitions, skinning, the node tree and any
  `HDT Skinned Mesh Physics Object` string stay exactly as the model has them. Only the vertex
  positions come from the player: vanilla sliders, RaceMenu sculpt and morphs, no expression (every
  expression, modifier and phoneme value is zeroed and the face morphs recomputed just before reading).
- Texture sets are baked like the Creation Kit does: the head part's TXST (the face uses the NPC's
  head texture when set), and the face's slot 6 points at the standard FaceTint path.
- The face tint is the player's rendered tint, saved uncompressed.
- Not carried over: RaceMenu overlays and tattoos, NiOverride node scales (the json lists the
  player's scales so a difference can be seen), and expressions.

An existing FaceGen file is overwritten only when this tool wrote it (its hash is in the json);
a file shipped by a mod is left alone and the export stops.

Apply the record data with the xEdit script `Atelier - Apply NPC Appearance.pas`: select the NPC
records and run it. It writes head parts, hair colour, head texture, texture lighting, face morphs,
face parts, tint layers, height and weight, and refuses a record whose race or sex differs from the
face or whose head parts are not in its plugin or its masters.

### A self-contained NPC mod: `Atelier - Build NPC Mod.pas`

To ship the character, select the NPC records in xEdit and run `Atelier - Build NPC Mod.pas`
instead. It asks for a plugin (a new ESL-flagged .esp, or a loaded plugin to add to), an EditorID
prefix, the output folder (the new mod's folder) and whether to make a new NPC or put the face on
the selected one, then:

- copies every head part of the face that is not from the base game into the plugin, with the
  extra parts, texture sets and colours they use, named `<prefix>_<EditorID>`; base-game parts
  (Skyrim, Update, the DLCs, Creation Club) stay references;
- moves the copies' meshes and TRI files to `meshes\<prefix>\<original path>`; textures keep
  their paths;
- writes the face into the NPC record, pointing at the copies;
- copies the FaceGen head and tint to the NPC's own FaceGen path, with the head's shapes renamed
  to the copies' EditorIDs and its tint slot pointed at the new tint;
- copies every file the copies use (meshes, TRI files, the textures each mesh and texture set
  names, SMP xml files a mesh links to; base-game archive files are left out), and the RaceMenu
  morphs of the copied parts (`morphs.ini` lines, `Races.ini`, slider files) so the character
  can still be edited in RaceMenu;
- writes `SKSE\Plugins\RaceMenuAtelier\Builds\<plugin>.json`: the records copied, every file
  with its MD5 and the Mod Organizer 2 mods that provide it, base-game files left out, and files
  that were not found. Use it to see whose permission a release needs;
- saves a new plugin straight into the output folder. When xEdit asks to save on exit, untick
  that plugin. A plugin that was already loaded is saved by xEdit as usual.

Running it again gives the same result: copies are found by EditorID and reused. A copy is not
refreshed from a changed source; delete the copy first. Both scripts need
`AtelierNPCFaceCore.pas` in the same `Edit Scripts` folder.

## Requirements

- Skyrim SE / AE with SKSE
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
- [RaceMenu](https://www.nexusmods.com/skyrimspecialedition/mods/19080)
- [SKSE Menu Framework](https://www.nexusmods.com/skyrimspecialedition/mods/120352) 3.x and its requirements

## Building

Requires Visual Studio 2022 (C++ workload) and [xmake](https://xmake.io) 3.0+.

```bat
git clone --recursive https://github.com/rustyoldsword/RaceMenuAtelier.git
cd RaceMenuAtelier
git -C lib/CommonLibSSE-NG apply ../../patches/CommonLibSSE-NG-TESObjectREFR-Unk_8C.patch
xmake f -m releasedbg --skyrim_vr=n
xmake build RaceMenuAtelier
powershell -ExecutionPolicy Bypass -File tools\package.ps1
```

`tools\package.ps1` stages `dist\RaceMenu Atelier\` in the Data layout and packs a `.7z` when 7-Zip
is installed. Set `RMA_DEPLOY_DIR` to a mod folder to have every build copied there.

The patch removes `TESObjectREFR::Unk_8C`, a declaration CommonLibSSE-NG 4.39.2 carries that is not a
virtual slot on SE/AE: without it every `TESObjectREFR` virtual after slot 0x8B (`Get3D` among them) is
called one slot off. Released DLLs from 1.1.0 on are built with it.

## Settings

`Data\SKSE\Plugins\RaceMenuAtelier.ini` - most options are also editable from the Options popup in game.

## Credits

- expired6978 - RaceMenu
- Thiago and Quantumyilmaz - SKSE Menu Framework
- The SKSE team, CommonLibSSE-NG contributors, Dear ImGui

## License

GPL-3.0, see [LICENSE](LICENSE). `lib/SKSE-Menu-Framework-3-API` keeps its own LGPL-2.1 license.
