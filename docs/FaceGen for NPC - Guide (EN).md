# RaceMenu Atelier: FaceGen for NPC — complete guide

Make a face in RaceMenu and put it on any NPC: no Creation Kit, no "Export FaceGen" (Ctrl+F4),
no hand-editing of records. This guide covers the whole path, from the face on your character to
a finished, self-contained follower mod.

---

## 1. What you get

The work is split between three tools:

| Tool | Where | What it does |
|---|---|---|
| **FaceGen for NPC** button | RaceMenu Atelier, in the game | Writes the face of your current character as the pre-built head (FaceGen) of an NPC you pick, plus a JSON file with all of the face's record data. |
| **Atelier - Apply NPC Appearance** | xEdit script | Writes that record data into the NPC: head parts, hair colour, skin tone, tint layers, face sliders, height and weight. |
| **Atelier - Build NPC Mod** | xEdit script | Builds a self-contained mod: copies the head parts the face uses into your plugin together with every file they need, writes the face into the NPC, copies the FaceGen, and writes a manifest of where every file came from. |

There are two ways to finish:

- **Route A: Apply.** The head parts are already in the NPC's plugin or its masters, for example
  your own follower mod that ships its hair and eyes. One script, and you move the FaceGen files.
- **Route B: Build.** You used head parts from other mods (hair packs, eyes, a high-poly head)
  and want one folder that works on its own. The script copies everything for you.

---

## 2. Requirements

- Skyrim Special Edition or Anniversary Edition with SKSE.
- RaceMenu, Address Library for SKSE Plugins, SKSE Menu Framework 3.x.
- RaceMenu Atelier **1.2.0 or newer** (the version with "FaceGen for NPC").
- **SSEEdit (xEdit) 4.1.5 or newer.**
- Mod Organizer 2 is recommended. Vortex works too, but the manifest can only list source mods
  under MO2.

### Installing the xEdit scripts

The scripts ship in the Atelier archive under `docs\RaceMenu Atelier - xEdit Scripts`
(under MO2: right-click the mod, Open in Explorer). Copy these three files into xEdit's
`Edit Scripts` folder (next to `SSEEdit.exe`):

- `Atelier - Apply NPC Appearance.pas`
- `Atelier - Build NPC Mod.pas`
- `AtelierNPCFaceCore.pas` (shared code; both scripts need it)

Under MO2, run xEdit from MO2 so it sees your whole load order, Overwrite included.

---

## 3. Before you start: the NPC record

The face is written **for a specific NPC record**, so that record has to exist and be loaded.

- **Your own new follower:** create the NPC in xEdit or the Creation Kit first: race, sex, name,
  anything. The face comes later. Or take any existing NPC of the right race and sex as a
  "carrier" and let Build make a new NPC from it (route B, "new NPC").
- **Race and sex must match your character.** The export only warns, but both scripts refuse a
  record whose race or sex differs from the face, because the head would not fit its head parts.
- **No template traits.** An NPC that takes its traits from a template shows the template's face.
  The picker draws such NPCs in grey.
- The NPC must come from a plugin. NPCs created at runtime cannot be targeted.

---

## 4. Step 1: make the face in RaceMenu

Open RaceMenu (`showracemenu` in the console) and build the face as usual: head, hair, eyes,
brows, sliders, sculpt, colours.

Things to know:

- **Head parts must come from plugins.** Every head part you pick is recorded by its plugin and
  FormID. Parts that only exist at runtime cannot be exported.
- **No need to reset the expression.** The export zeroes every expression, modifier and phoneme
  and recomputes the face before reading it.
- **The hair colour is baked in.** The game does not recolour a loaded FaceGen head, so the
  export copies your live hair colour into the head's hair shaders and also writes it into the record.
- **Not carried over:** RaceMenu overlays and tattoos, NiOverride node scales, expressions. The
  JSON lists your scales so that you can see a difference.
- **Height and weight are taken from your character.** Change them in the record afterwards if
  the NPC should differ.

---

## 5. Step 2: export the face

1. In RaceMenu open Atelier's **Tools** section and press **FaceGen for NPC**.
2. Press **Choose the NPC...**. A window lists every NPC record of the loaded plugins:
   - the plugin list on the right has its own search box and shows how many NPCs each plugin has;
   - the main search finds a name, an EditorID or a FormID: the local one (`D62`) or the runtime
     one the console prints (`FE17ED62`);
   - grey rows are NPCs that take their face from a template.
3. Click the NPC. The button now shows it, for example `Rem (MyFollowers.esp|D62)`.
4. Press **Write**. Watch the status line. When it is done, it reads
   `FaceGen for <plugin>|<id> written: N shapes, tint ...`.

Three files are written. Under MO2 they land in **Overwrite**. A file whose folder already exists
in a mod may be written into that mod instead:

| File | Content |
|---|---|
| `meshes\actors\character\FaceGenData\FaceGeom\<plugin>\<id>.nif` | the head |
| `textures\actors\character\FaceGenData\FaceTint\<plugin>\<id>.dds` | the face tint |
| `SKSE\Plugins\RaceMenuAtelier\NPCFaces\<plugin>\<id>.json` | the record data for xEdit |

`<plugin>` is the plugin that defines the NPC and `<id>` its local FormID in eight hex digits
(a light plugin uses its 12-bit ID, for example `00000D62`).

**Safety:** an existing FaceGen file is overwritten only if Atelier wrote it (its hash is in the
JSON). A FaceGen file shipped by a mod is left alone and the export stops with a message.

The log `Documents\My Games\Skyrim Special Edition\SKSE\RaceMenuAtelier.log` lists every shape
written and anything that was left out.

You can export several NPCs in one session. Change the face, pick the next NPC, press Write.

---

## 6a. Route A: apply to the NPC (head parts already in the plugin)

Use this when every head part of the face is defined in the NPC's plugin or one of its masters.

1. Close the game (or leave it at the main menu) and start xEdit with the NPC's plugin.
2. Select the NPC record or records (the original or an override), right-click,
   **Apply Script...**, choose **Atelier - Apply NPC Appearance**.
3. Read the messages. `applied: 14 head parts, 33 tint layers, height 1` means it worked.
   `REFUSED` lists the reason (race, sex, or a head part from a plugin that is not a master).
4. Save the plugin when xEdit asks on exit.
5. **Move the FaceGen files** (`.nif` and `.dds`) from Overwrite into your mod folder, keeping the
   folder structure. The JSON is only needed by the scripts. Keep it if you want to export the
   same NPC again later, because it is what lets Atelier overwrite its own FaceGen.
6. Adjust height or weight in the record if the NPC should differ from your character.

What is written: PNAM head parts, HCLF hair colour (a new `RMA_HairColor_<id>` colour record when
the colour form is not reachable from the plugin), FTST head texture, QNAM texture lighting, NAM9
face morphs, NAMA face parts, the tint layers, NAM6 height and NAM7 weight. Running it again with
the same JSON gives the same record.

---

## 6b. Route B: build a self-contained mod

Use this when the face uses head parts from other mods, or whenever you want one folder that
needs nothing else.

### Run it

1. Start xEdit (under MO2, from MO2) with your load order.
2. Select the NPC record or records the faces were exported for, right-click,
   **Apply Script...**, choose **Atelier - Build NPC Mod**.
3. Answer four questions:

   | Question | What to enter |
   |---|---|
   | **Plugin** | A new name (`MyFollower.esp`) makes a new ESL-flagged plugin. The name of a loaded plugin (for example the NPC's own) adds to it. |
   | **EditorID prefix** | Letters, digits and `_`, for example `MYF`. Copies are named `MYF_<original EditorID>` and their meshes go to `meshes\MYF\`. |
   | **Output folder** | The new mod's folder. Under MO2 the default is `mods\<plugin name>`, which MO2 shows as a new mod after a refresh (F5). |
   | **New NPC?** | **Yes**: a new NPC record `<prefix>_<EditorID>` is created in the plugin from the selected one and gets the face. **No**: the selected NPC gets the face, as an override in the plugin when it is defined elsewhere. |

4. When it finishes, the messages show the summary: records copied, files copied, base-game files
   left out, files missing, and the manifest path.
5. **On exit, when xEdit asks which plugins to save, untick the new plugin.** It is already saved
   in the output folder. If you save it, an identical copy also lands in Overwrite. A plugin that
   already existed is saved by xEdit as usual, so tick that one.

### What it does

For every selected NPC with an exported face:

1. **Copies the head parts.** Every head part of the face that does not come from the base game
   is copied into the plugin, together with what it points at: extra parts, texture sets, colours.
   Base-game parts (Skyrim, Update, the three DLCs, Creation Club) stay references. Copies are
   named `<prefix>_<original EditorID>`, so they never clash with the originals. Duplicate head
   part EditorIDs can crash the game.
2. **Moves the meshes.** The copies' meshes and TRI files go to `meshes\<prefix>\<original path>`.
   Textures keep their paths.
3. **Writes the face** into the NPC, pointing at the copies (as route A does).
4. **Copies the FaceGen** to the NPC's own FaceGen path. The head's shapes are renamed to the copies'
   EditorIDs (the game matches FaceGen shapes to head parts by EditorID) and the head's tint slot
   points at the new tint. You do not need to export again.

Then, once for all NPCs:

5. **Copies every file the copies use:** meshes, TRI files, every texture each mesh and texture
   set names, and SMP `.xml` files a mesh links to. It also copies the textures the FaceGen head
   uses, such as a custom skin. Files that the base game's archives provide are left out.
6. **Copies the RaceMenu morphs** of the copied head parts: the matching `morphs.ini` lines,
   rewritten for the new paths, the morph TRI files, and the source plugin's `Races.ini` and slider
   files. The character can still be edited in RaceMenu with the copies.
7. **Writes the manifest** `SKSE\Plugins\RaceMenuAtelier\Builds\<plugin>.json`.

### Read the manifest

| Key | Meaning |
|---|---|
| `records` | every record copied (`original -> copy`) |
| `npcs` | each NPC, its FaceGen paths and shape count; `shapesWithoutPart` must be absent |
| `files` | every file copied: path, source path, archive or `loose`, **MD5**, and **providers**, the MO2 mods that contain it (highest priority first) |
| `baseGameFiles` | files left out because the base game provides them |
| `missing` | files a record or mesh names but nothing provides. **Fix these before release.** |
| `notes` | which `morphs.ini` files the morphs came from, and warnings |

`providers` tells you whose permission a release needs.

### After the build

1. In MO2 press F5 and enable the new mod. Put it below the mods it was built from, or above if you
   later disable them.
2. Enable the plugin and sort it (LOOT).
3. Test on a **new game** or with `coc` into a test cell. Spawn the NPC with
   `player.placeatme <FormID>` and check the face, hair colour and eyes up close.
4. When everything works, you can disable the source mods. The new folder has its own copies.

### Running it again

Running Build again with the same answers gives the same result: copies are found by EditorID and
reused, and files are rewritten. A copy is **not** refreshed if its source record changed. Delete the
copy in xEdit first and the next run takes the source again.

### Presets (optional)

Put `Atelier - Build NPC Mod.json` next to the script to preset the answers, or to skip the dialogs:

```json
{
  "plugin": "MyFollower.esp",
  "prefix": "MYF",
  "outputFolder": "D:\\Modding\\MO2\\mods\\MyFollower",
  "newNpc": true,
  "mo2Folder": "D:\\Modding\\MO2",
  "interactive": true
}
```

`interactive: false` runs without dialogs. `mo2Folder` is only needed when the script cannot find
`ModOrganizer.ini` two folders above the game's Data folder (a portable instance is found by itself).

---

## 7. Troubleshooting

| Problem | Cause and fix |
|---|---|
| **Dark or grey face** | The FaceGen head does not match the record. Each head shape must be named after a head part the NPC really has, and the files must sit at `<plugin>\<id>` of the NPC's defining plugin. Re-export, run the script again, and check that the FaceGen files won in MO2 (Overwrite or your mod, not an older copy). |
| **White hair** | Old Atelier builds before 1.2.0 did not bake the hair colour. Export again. |
| Export: `... already exists and was not written by this tool` | A mod ships FaceGen for that NPC. Move that file out of its mod (or hide it in MO2), then export. |
| Export: `That NPC was created at runtime` | Pick an NPC defined in a plugin. |
| Export warns `race differs` or `sex differs` | Switch your character to the NPC's race and sex. The scripts refuse a mismatch. |
| Export warns `uses a template's traits` | The game shows the template's face. Remove the Traits template flag or target the template. |
| Apply: `REFUSED ... head part ... is not a master` | The part lives in a mod your plugin does not depend on. Use route B, or add that plugin as a master. |
| Apply or Build: `no exported face` | No JSON for that NPC. Check that you exported this exact NPC and that xEdit sees Overwrite (start it from MO2). |
| Build note: `FaceGen shape X is not a head part of the NPC` | The face was exported with different head parts than the record now has. Export again, then build again. |
| Build: entries in `missing` | A record or mesh names a file nobody provides (often a broken path in the source mod). Find the file or fix the path. |
| Atelier will not overwrite a built NPC's FaceGen | That FaceGen came from Build and has no Atelier JSON for the new NPC. Export to the original NPC and build again, or delete the built FaceGen files first. |
| A new plugin shows no records in game | An empty plugin of the same name in Overwrite hides it. Delete it from Overwrite (see step 5 of "Run it"). |

---

## 8. Limits

- RaceMenu overlays, tattoos and node scales are not carried over.
- Body morphs (BodySlide or OBody) are not part of the face. Set the NPC's body separately.
- Build copies head parts, texture sets and colours. Other records the NPC points at (outfit,
  voice, class, race) stay references to their plugins.
- Copies are not refreshed automatically when their source changes (see "Running it again").
- Releasing a built mod still needs the permission of every mod listed under `providers`.
