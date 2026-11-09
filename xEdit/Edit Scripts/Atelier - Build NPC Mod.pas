{
  Atelier - Build NPC Mod

  Builds a self-contained mod from faces exported by RaceMenu Atelier
  ("FaceGen for NPC"). Select the NPC records the faces were exported for
  and apply this script.

  It asks for:
    - the plugin: a new light plugin (ESL-flagged .esp), or a loaded plugin to
      add to (for example the NPC's own plugin);
    - an EditorID prefix for the copies (letters, digits, underscore);
    - the output folder: the new mod's folder (with MO2, a folder in mods\);
    - whether to make a NEW NPC from each selected NPC or to put the face on
      the selected NPC itself.

  For every selected NPC that has an exported face it:
    1. copies every head part of the face that does not come from the base
       game (Skyrim, Update, the three DLCs, Creation Club) into the plugin,
       with everything those parts point at that is a head part, texture set
       or colour (extra parts, texture sets, colours). Copies are named
       <prefix>_<original EditorID>; parts from the base game stay references;
    2. points the copies' meshes and TRI files at meshes\<prefix>\<original
       path>; textures keep their paths;
    3. writes the face into the NPC record (as "Atelier - Apply NPC
       Appearance" does), pointing at the copies;
    4. copies the FaceGen head and tint into the output folder at the NPC's
       own FaceGen path, renaming the head's shapes to the copies' EditorIDs
       (the game matches FaceGen shapes to head parts by EditorID) and
       pointing the head's tint slot at the new tint;
  and once at the end:
    5. copies every asset the copies use: meshes, TRI files, the textures each
       mesh and texture set names, SMP xml files a mesh links to. Files in the
       base game archives are not copied;
    6. copies the RaceMenu morphs (morphs.ini extensions) of the copied
       head parts, with the plugin's Races.ini and slider files, so the
       character can still be edited in RaceMenu;
    7. writes SKSE\Plugins\RaceMenuAtelier\Builds\<plugin>.json: every record
       copied, every file copied with its MD5 and the mods that provide it
       (read from Mod Organizer 2 when found), base-game files left out, and
       files that were not found;
    8. writes a new plugin into the output folder (a plugin that was already
       loaded is saved by xEdit as usual).

  Running it again gives the same result: copies are found by EditorID and
  reused, files are rewritten. A copy is not refreshed from a changed source
  record; delete the copy first to take the source again.

  Options can be preset in "Atelier - Build NPC Mod.json" next to this
  script, a json object with the keys plugin, prefix, outputFolder, newNpc,
  mo2Folder and interactive.

  Needs AtelierNPCFaceCore.pas in the same folder.
}
unit AtelierBuildNPCMod;

uses AtelierNPCFaceCore;

const
  OptionsFileName = 'Atelier - Build NPC Mod.json';
  MorphsRoot = 'meshes\actors\character\facegenmorphs\';
  MorphsTris = 'meshes\actors\character\facegenmorphs\morphs\';
  BlockBegin = '; RaceMenu Atelier - Build NPC Mod: begin';
  BlockEnd = '; RaceMenu Atelier - Build NPC Mod: end';

var
  TargetFile: IInterface;
  TargetIsNew, OptNewNpc, Aborted: Boolean;
  OptPlugin, OptPrefix, OptOut, OptMO2, ModsDir, OverwriteDir: string;
  RecMap: TStringList;    // original load-order hex = copy's load-order hex
  EdidMap: TStringList;   // original EditorID = copy's EditorID
  PathMap: TStringList;   // original 'meshes\...' TRI path = copy's path relative to meshes\
  Assets: TStringList;    // destination Data path = source Data path
  OwnSeen: TStringList;   // records of the target plugin already queued
  ModList: TStringList;   // enabled MO2 mods, highest priority first
  RecordsLog, NpcLog, GameFiles, Missing, Notes: TStringList;
  Manifest: TJsonObject;

// -------------------------------------------------------------------------
// small helpers

function YesNo(b: Boolean): string;
begin
  if b then Result := 'yes' else Result := 'no';
end;

function AddSlash(s: string): string;
begin
  Result := s;
  if (Result <> '') and (Copy(Result, Length(Result), 1) <> '\') then
    Result := Result + '\';
end;

function NoSlash(s: string): string;
begin
  Result := s;
  while (Result <> '') and (Copy(Result, Length(Result), 1) = '\') do
    Result := Copy(Result, 1, Length(Result) - 1);
end;

function NewSet: TStringList;
begin
  Result := TStringList.Create;
  Result.Sorted := True;
  Result.Duplicates := dupIgnore;
end;

function IsGamePlugin(aName: string): Boolean;
var
  n: string;
begin
  n := LowerCase(aName);
  Result := (n = 'skyrim.esm') or (n = 'update.esm') or (n = 'dawnguard.esm') or
    (n = 'hearthfires.esm') or (n = 'dragonborn.esm') or (Copy(n, 1, 2) = 'cc');
end;

function IsGameContainer(aContainer: string): Boolean;
var
  n: string;
begin
  n := LowerCase(ExtractFileName(aContainer));
  Result := (Pos('skyrim - ', n) = 1) or (n = 'update.bsa') or (n = 'dawnguard.bsa') or
    (n = 'hearthfires.bsa') or (n = 'dragonborn.bsa') or (n = '_resourcepack.bsa') or
    (n = 'marketplacetextures.bsa') or ((Copy(n, 1, 2) = 'cc') and (ExtractFileExt(n) = '.bsa'));
end;

function ValidPrefix(s: string): Boolean;
var
  i: Integer;
  c: string;
begin
  Result := s <> '';
  for i := 1 to Length(s) do begin
    c := Copy(s, i, 1);
    if Pos(UpperCase(c), 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_') = 0 then
      Result := False;
  end;
end;

function DefaultPrefix(aPlugin: string): string;
var
  i: Integer;
  s, c: string;
  newWord: Boolean;
begin
  s := ChangeFileExt(aPlugin, '');
  Result := '';
  newWord := True;
  for i := 1 to Length(s) do begin
    c := Copy(s, i, 1);
    if Pos(UpperCase(c), 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789') = 0 then
      newWord := True
    else begin
      if newWord then
        Result := Result + UpperCase(c);
      newWord := False;
    end;
  end;
  if Result = '' then
    Result := 'NPC';
end;

function NewEdid(aEdid: string): string;
begin
  if Pos(UpperCase(OptPrefix + '_'), UpperCase(aEdid)) = 1 then
    Result := aEdid
  else
    Result := OptPrefix + '_' + aEdid;
end;

// a mesh path relative to meshes\, moved under meshes\<prefix>\
function PrefixRel(aRel: string): string;
begin
  Result := aRel;
  if Pos('meshes\', LowerCase(Result)) = 1 then
    Result := Copy(Result, 8, Length(Result));
  if Pos(LowerCase(OptPrefix + '\'), LowerCase(Result)) <> 1 then
    Result := OptPrefix + '\' + Result;
end;

function MeshRel(aRel: string): string;
begin
  Result := aRel;
  if Pos('meshes\', LowerCase(Result)) = 1 then
    Result := Copy(Result, 8, Length(Result));
end;

// splits "a, b, c" (paths may contain spaces)
procedure SplitCommas(aText: string; aOut: TStringList);
var
  p: Integer;
  s: string;
begin
  aOut.Clear;
  s := aText;
  p := Pos(',', s);
  while p > 0 do begin
    if Trim(Copy(s, 1, p - 1)) <> '' then
      aOut.Add(Trim(Copy(s, 1, p - 1)));
    s := Copy(s, p + 1, Length(s));
    p := Pos(',', s);
  end;
  if Trim(s) <> '' then
    aOut.Add(Trim(s));
end;

procedure LoadResourceLines(aPath: string; aOut: TStringList);
var
  bs: TBytesStream;
begin
  aOut.Clear;
  bs := TBytesStream.Create(ResourceOpenData('', aPath));
  try
    aOut.LoadFromStream(bs);
  finally
    bs.Free;
  end;
end;

procedure QueueAsset(aDest, aSrc: string);
begin
  if (aDest = '') or (aSrc = '') then
    Exit;
  if Assets.IndexOfName(aDest) < 0 then
    Assets.Add(aDest + '=' + aSrc);
end;

procedure QueueTexture(aValue: string);
var
  t: string;
begin
  if Trim(aValue) = '' then
    Exit;
  t := wbNormalizeResourceName(Trim(aValue), resTexture);
  QueueAsset(t, t);
end;

// a mesh and everything it names: textures, SMP xml
procedure QueueNif(aSrc, aDest: string);
var
  nif: TwbNifFile;
  b: TwbNifBlock;
  el: TdfElement;
  i, j: Integer;
  s: string;
begin
  QueueAsset(aDest, aSrc);
  if not ResourceExists(aSrc) then
    Exit;
  nif := TwbNifFile.Create;
  try
    try
      nif.LoadFromResource(aSrc);
      for i := 0 to Pred(nif.BlocksCount) do begin
        b := nif.Blocks[i];
        if b.BlockType = 'BSShaderTextureSet' then begin
          el := b.Elements['Textures'];
          for j := 0 to Pred(el.Count) do
            QueueTexture(el[j].EditValue);
        end
        else if b.BlockType = 'BSEffectShaderProperty' then begin
          QueueTexture(b.EditValues['Source Texture']);
          QueueTexture(b.EditValues['Grayscale Texture']);
          QueueTexture(b.EditValues['Env Map Texture']);
          QueueTexture(b.EditValues['Normal Texture']);
          QueueTexture(b.EditValues['Env Mask Texture']);
        end
        else if b.BlockType = 'NiStringExtraData' then begin
          if Pos('hdt', LowerCase(b.EditValues['Name'])) > 0 then begin
            s := Trim(b.EditValues['String Data']);
            if s <> '' then
              QueueAsset(s, s);
          end;
        end;
      end;
    except
      on E: Exception do
        Notes.Add('could not read ' + aSrc + ': ' + E.Message);
    end;
  finally
    nif.Free;
  end;
end;

// assets of a record; aRewrite moves meshes and TRIs of copy n under the prefix
procedure QueueRecordAssets(src, n: IInterface; aRewrite: Boolean);
var
  sp, dp: IInterface;
  i: Integer;
  s, rel: string;
begin
  if Signature(src) = 'HDPT' then begin
    s := MeshRel(GetElementEditValues(src, 'Model\MODL'));
    if s <> '' then begin
      rel := s;
      if aRewrite then begin
        rel := PrefixRel(s);
        SetElementEditValues(n, 'Model\MODL', rel);
      end;
      QueueNif('meshes\' + s, 'meshes\' + rel);
    end;
    sp := ElementByName(src, 'Parts');
    dp := ElementByName(n, 'Parts');
    for i := 0 to Pred(ElementCount(sp)) do begin
      s := MeshRel(GetElementEditValues(ElementByIndex(sp, i), 'NAM1'));
      if s = '' then
        Continue;
      rel := s;
      if aRewrite then begin
        rel := PrefixRel(s);
        SetElementEditValues(ElementByIndex(dp, i), 'NAM1', rel);
      end;
      QueueAsset('meshes\' + rel, 'meshes\' + s);
      PathMap.Values[LowerCase('meshes\' + s)] := rel;
    end;
  end
  else if Signature(src) = 'TXST' then begin
    for i := 0 to 7 do
      QueueTexture(GetElementEditValues(src, 'Textures (RGB/A)\TX0' + IntToStr(i)));
  end;
end;

function IsCopyable(aSig: string): Boolean;
begin
  Result := (aSig = 'HDPT') or (aSig = 'TXST') or (aSig = 'CLFM');
end;

// -------------------------------------------------------------------------
// records

function CopyRecord(aRec: IInterface): IInterface;
var
  m, src, n, grp, srcEl, el, entry, link, c: IInterface;
  key, edid, sig: string;
  i, k: Integer;
  hexes: TStringList;
begin
  m := MasterOrSelf(aRec);
  Result := m;
  if not IsCopyable(Signature(m)) then
    Exit;
  if IsGamePlugin(GetFileName(GetFile(m))) then
    Exit;
  src := WinningOverride(m);

  // already in the target plugin: queue its assets as they are
  if Equals(GetFile(m), TargetFile) then begin
    key := LoadOrderHex(m);
    if OwnSeen.IndexOf(key) < 0 then begin
      OwnSeen.Add(key);
      QueueRecordAssets(src, src, False);
      if Signature(m) = 'HDPT' then begin
        srcEl := ElementByName(src, 'Extra Parts');
        for i := 0 to Pred(ElementCount(srcEl)) do
          CopyRecord(LinksTo(ElementByIndex(srcEl, i)));
        if Assigned(LinksTo(ElementBySignature(src, 'TNAM'))) then
          CopyRecord(LinksTo(ElementBySignature(src, 'TNAM')));
      end;
    end;
    Exit;
  end;

  key := LoadOrderHex(m);
  edid := NewEdid(EditorID(m));
  grp := GroupBySignature(TargetFile, Signature(m));
  n := nil;
  if Assigned(grp) then
    n := MainRecordByEditorID(grp, edid);
  if (RecMap.Values[key] <> '') and Assigned(n) then begin
    Result := n;
    Exit;
  end;
  if not Assigned(n) then begin
    AddRequiredElementMasters(src, TargetFile, False);
    n := wbCopyElementToFile(src, TargetFile, True, True);
    SetEditorID(n, edid);
    RecordsLog.Add(Name(src) + ' -> ' + Name(n));
  end else
    RecordsLog.Add(Name(src) + ' -> ' + Name(n) + ' (existing copy reused)');
  RecMap.Values[key] := LoadOrderHex(n);
  EdidMap.Values[EditorID(m)] := edid;
  Result := n;

  if Signature(n) = 'HDPT' then begin
    // extra parts: xEdit keeps this list sorted, so it is written anew
    // rather than edited by index
    srcEl := ElementByName(src, 'Extra Parts');
    if ElementCount(srcEl) > 0 then begin
      hexes := TStringList.Create;
      try
        for i := 0 to Pred(ElementCount(srcEl)) do begin
          c := CopyRecord(LinksTo(ElementByIndex(srcEl, i)));
          hexes.Add(LoadOrderHex(c));
        end;
        RemoveElement(n, 'Extra Parts');
        el := Add(n, 'Extra Parts', True);
        for i := 0 to Pred(hexes.Count) do begin
          if i = 0 then
            entry := ElementByIndex(el, 0)
          else
            entry := ElementAssign(el, HighInteger, nil, False);
          SetEditValue(entry, hexes[i]);
        end;
      finally
        hexes.Free;
      end;
    end;
    // texture set and colour
    for k := 0 to 1 do begin
      if k = 0 then sig := 'TNAM' else sig := 'CNAM';
      link := LinksTo(ElementBySignature(src, sig));
      if Assigned(link) then begin
        c := CopyRecord(link);
        SetElementEditValues(n, sig, LoadOrderHex(c));
      end;
    end;
  end;
  QueueRecordAssets(src, n, True);
end;

function TargetNpc(e: IInterface): IInterface;
var
  m, grp, n: IInterface;
  i: Integer;
  edid: string;
begin
  m := MasterOrSelf(e);
  if OptNewNpc then begin
    edid := NewEdid(EditorID(m));
    grp := GroupBySignature(TargetFile, 'NPC_');
    n := nil;
    if Assigned(grp) then
      n := MainRecordByEditorID(grp, edid);
    if not Assigned(n) then begin
      AddRequiredElementMasters(e, TargetFile, False);
      n := wbCopyElementToFile(e, TargetFile, True, True);
      SetEditorID(n, edid);
      RecordsLog.Add(Name(e) + ' -> ' + Name(n) + ' (new NPC)');
    end;
    Result := n;
    Exit;
  end;
  if Equals(GetFile(m), TargetFile) then begin
    Result := m;
    Exit;
  end;
  for i := 0 to Pred(OverrideCount(m)) do
    if Equals(GetFile(OverrideByIndex(m, i)), TargetFile) then begin
      Result := OverrideByIndex(m, i);
      Exit;
    end;
  AddRequiredElementMasters(e, TargetFile, False);
  Result := wbCopyElementToFile(e, TargetFile, False, True);
  RecordsLog.Add(Name(e) + ' -> override in ' + GetFileName(TargetFile));
end;

// EditorIDs the FaceGen head of an NPC must use as shape names
procedure CollectPartEdids(aPart: IInterface; aOut: TStringList);
var
  ex: IInterface;
  i: Integer;
begin
  if not Assigned(aPart) then
    Exit;
  aPart := WinningOverride(aPart);
  aOut.Add(EditorID(aPart));
  ex := ElementByName(aPart, 'Extra Parts');
  for i := 0 to Pred(ElementCount(ex)) do
    CollectPartEdids(LinksTo(ElementByIndex(ex, i)), aOut);
end;

// -------------------------------------------------------------------------
// FaceGen

procedure CopyFaceGen(aDoc: TJsonObject; aNpc: IInterface; aLog: TJsonObject);
var
  nif: TwbNifFile;
  b: TwbNifBlock;
  el: TdfElement;
  bs: TBytesStream;
  expected, shapes: TStringList;
  hp: IInterface;
  i, j: Integer;
  srcNif, srcTint, dstNif, dstTint, s, newName: string;
begin
  srcNif := FaceGeomFolder + aDoc.O['npc'].S['plugin'] + '\' + aDoc.O['npc'].S['formID'] + '.nif';
  srcTint := FaceTintFolder + aDoc.O['npc'].S['plugin'] + '\' + aDoc.O['npc'].S['formID'] + '.dds';
  dstNif := FaceGeomFolder + GetFileName(GetFile(MasterOrSelf(aNpc))) + '\' + LocalHex(aNpc) + '.nif';
  dstTint := FaceTintFolder + GetFileName(GetFile(MasterOrSelf(aNpc))) + '\' + LocalHex(aNpc) + '.dds';
  aLog.S['faceGeom'] := dstNif;
  aLog.S['faceTint'] := dstTint;
  if not ResourceExists(srcNif) then begin
    Missing.Add(srcNif + '  (FaceGen head of ' + Name(aNpc) + ')');
    Exit;
  end;

  expected := NewSet;
  shapes := NewSet;
  nif := TwbNifFile.Create;
  try
    hp := ElementByName(aNpc, 'Head Parts');
    for i := 0 to Pred(ElementCount(hp)) do
      CollectPartEdids(LinksTo(ElementByIndex(hp, i)), expected);

    nif.LoadFromResource(srcNif);
    for i := 0 to Pred(nif.BlocksCount) do begin
      b := nif.Blocks[i];
      if (b.BlockType = 'BSDynamicTriShape') or (b.BlockType = 'BSTriShape') or (b.BlockType = 'NiTriShape') then begin
        newName := EdidMap.Values[b.EditValues['Name']];
        if newName <> '' then
          b.EditValues['Name'] := newName;
        shapes.Add(b.EditValues['Name']);
      end
      else if b.BlockType = 'BSShaderTextureSet' then begin
        el := b.Elements['Textures'];
        for j := 0 to Pred(el.Count) do begin
          s := Trim(el[j].EditValue);
          if s = '' then
            Continue;
          if SameText(wbNormalizeResourceName(s, resTexture), wbNormalizeResourceName(srcTint, resTexture)) then
            el[j].EditValue := LowerCase(dstTint)
          else
            QueueTexture(s);
        end;
      end;
    end;
    ForceDirectories(ExtractFilePath(OptOut + dstNif));
    nif.SaveToFile(OptOut + dstNif);

    for i := 0 to Pred(shapes.Count) do
      if expected.IndexOf(shapes[i]) < 0 then begin
        Notes.Add(Name(aNpc) + ': FaceGen shape ' + shapes[i] + ' is not a head part of the NPC - the face will be dark until it is exported again');
        aLog.A['shapesWithoutPart'].Add(shapes[i]);
      end;
    aLog.I['shapes'] := shapes.Count;
  finally
    nif.Free;
    expected.Free;
    shapes.Free;
  end;

  if ResourceExists(srcTint) then begin
    bs := TBytesStream.Create(ResourceOpenData('', srcTint));
    try
      ForceDirectories(ExtractFilePath(OptOut + dstTint));
      bs.SaveToFile(OptOut + dstTint);
    finally
      bs.Free;
    end;
  end else
    Missing.Add(srcTint + '  (face tint of ' + Name(aNpc) + ')');
end;

// -------------------------------------------------------------------------
// RaceMenu morphs

procedure MergeBlock(aPath: string; aLines: TStringList; aHeader: string);
var
  text: TStringList;
  i: Integer;
  inBlock: Boolean;
begin
  text := TStringList.Create;
  try
    if FileExists(aPath) then
      text.LoadFromFile(aPath)
    else if aHeader <> '' then
      text.Text := aHeader;
    i := 0;
    inBlock := False;
    while i < text.Count do begin
      if Trim(text[i]) = BlockBegin then
        inBlock := True;
      if inBlock then begin
        if Trim(text[i]) = BlockEnd then
          inBlock := False;
        text.Delete(i);
      end else
        Inc(i);
    end;
    if aLines.Count > 0 then begin
      text.Add(BlockBegin);
      text.AddStrings(aLines);
      text.Add(BlockEnd);
    end;
    ForceDirectories(ExtractFilePath(aPath));
    text.SaveToFile(aPath);
  finally
    text.Free;
  end;
end;

procedure BuildMorphs;
var
  all, ini, items, ours, races, sliderFiles: TStringList;
  i, j, k: Integer;
  p, low, dir, line, newSrc, newLine, ourDir: string;
  contributed: Boolean;
begin
  if PathMap.Count = 0 then
    Exit;
  all := TStringList.Create;
  ini := TStringList.Create;
  items := TStringList.Create;
  ours := TStringList.Create;
  races := TStringList.Create;
  sliderFiles := NewSet;
  try
    ResourceList('', all);
    for i := 0 to Pred(all.Count) do
      all[i] := LowerCase(all[i]);
    ourDir := MorphsRoot + LowerCase(GetFileName(TargetFile)) + '\';
    for i := 0 to Pred(all.Count) do begin
      p := all[i];
      if (Pos(MorphsRoot, p) <> 1) or (ExtractFileName(p) <> 'morphs.ini') then
        Continue;
      dir := ExtractFilePath(p);
      if SameText(dir, ourDir) then
        Continue;
      LoadResourceLines(p, ini);
      contributed := False;
      for j := 0 to Pred(ini.Count) do begin
        line := Trim(ini[j]);
        if (Pos('extension', LowerCase(line)) <> 1) or (Pos('=', line) = 0) then
          Continue;
        SplitCommas(Copy(line, Pos('=', line) + 1, Length(line)), items);
        if items.Count < 2 then
          Continue;
        newSrc := PathMap.Values[LowerCase('meshes\' + MeshRel(items[0]))];
        if newSrc = '' then
          Continue;
        newLine := 'extension = ' + newSrc;
        for k := 1 to Pred(items.Count) do begin
          QueueAsset(MorphsTris + OptPrefix + '\' + items[k], MorphsTris + items[k]);
          newLine := newLine + ', ' + OptPrefix + '\' + items[k];
        end;
        if ours.IndexOf(newLine) < 0 then
          ours.Add(newLine);
        contributed := True;
      end;
      if contributed then begin
        Notes.Add('RaceMenu morphs taken from ' + p);
        if all.IndexOf(dir + 'races.ini') >= 0 then begin
          LoadResourceLines(dir + 'races.ini', ini);
          for j := 0 to Pred(ini.Count) do
            if (Trim(ini[j]) <> '') and (races.IndexOf(Trim(ini[j])) < 0) then
              races.Add(Trim(ini[j]));
        end;
        for j := 0 to Pred(all.Count) do
          if Pos(dir + 'sliders\', all[j]) = 1 then
            QueueAsset(ourDir + 'sliders\' + ExtractFileName(all[j]), all[j]);
      end;
    end;
    if (ours.Count > 0) or FileExists(OptOut + ourDir + 'morphs.ini') then
      MergeBlock(OptOut + ourDir + 'morphs.ini', ours,
      '# source TRI, additional TRI'#13#10'# Source TRI relative to Meshes\'#13#10'# Additional TRI relative to Meshes\actors\character\FaceGenMorphs\morphs\');
    if races.Count > 0 then
      MergeBlock(OptOut + ourDir + 'races.ini', races, '');
    Manifest.I['morphLines'] := ours.Count;
  finally
    all.Free;
    ini.Free;
    items.Free;
    ours.Free;
    races.Free;
    sliderFiles.Free;
  end;
end;

// -------------------------------------------------------------------------
// Mod Organizer 2 (only for the manifest's "providers")

procedure LoadModList;
var
  ini: TMemIniFile;
  lines: TStringList;
  profile, s: string;
  i: Integer;
begin
  ModList := TStringList.Create;
  if OptMO2 = '' then begin
    s := ExtractFilePath(NoSlash(ExtractFilePath(NoSlash(DataPath))));
    if FileExists(s + 'ModOrganizer.ini') then
      OptMO2 := s;
  end;
  if OptMO2 = '' then
    Exit;
  OptMO2 := AddSlash(OptMO2);
  if not FileExists(OptMO2 + 'ModOrganizer.ini') then begin
    Notes.Add('no ModOrganizer.ini in ' + OptMO2 + '; providers not listed');
    OptMO2 := '';
    Exit;
  end;
  ini := TMemIniFile.Create(OptMO2 + 'ModOrganizer.ini');
  try
    profile := ini.ReadString('General', 'selected_profile', 'Default');
    ModsDir := ini.ReadString('Settings', 'mod_directory', '');
  finally
    ini.Free;
  end;
  if Pos('@ByteArray(', profile) = 1 then
    profile := Copy(profile, 12, Length(profile) - 12);
  if ModsDir = '' then
    ModsDir := OptMO2 + 'mods\';
  ModsDir := AddSlash(StringReplace(ModsDir, '/', '\', [rfReplaceAll]));
  OverwriteDir := OptMO2 + 'overwrite\';
  lines := TStringList.Create;
  try
    if FileExists(OptMO2 + 'profiles\' + profile + '\modlist.txt') then
      lines.LoadFromFile(OptMO2 + 'profiles\' + profile + '\modlist.txt');
    for i := 0 to Pred(lines.Count) do
      if (Copy(lines[i], 1, 1) = '+') and (Pos('_separator', lines[i]) = 0) then
        ModList.Add(Copy(lines[i], 2, Length(lines[i])));
  finally
    lines.Free;
  end;
  Manifest.S['mo2Profile'] := profile;
end;

procedure AddProviders(aPath: string; aArr: TJsonArray);
var
  i: Integer;
begin
  if OptMO2 = '' then
    Exit;
  if FileExists(OverwriteDir + aPath) then
    aArr.Add('Overwrite');
  for i := 0 to Pred(ModList.Count) do
    if FileExists(ModsDir + ModList[i] + '\' + aPath) then
      aArr.Add(ModList[i]);
end;

// -------------------------------------------------------------------------

procedure CopyAssets;
var
  sl: TStringList;
  bs: TBytesStream;
  i, k: Integer;
  dest, src, container, outPath: string;
  game: Boolean;
  o: TJsonObject;
begin
  sl := TStringList.Create;
  try
    for i := 0 to Pred(Assets.Count) do begin
      dest := Assets.Names[i];
      src := Assets.ValueFromIndex[i];
      if not ResourceExists(src) then begin
        Missing.Add(src);
        Continue;
      end;
      sl.Clear;
      ResourceCount(src, sl);
      game := False;
      for k := 0 to Pred(sl.Count) do
        if IsGameContainer(sl[k]) then
          game := True;
      if game then begin
        GameFiles.Add(src);
        Continue;
      end;
      container := sl[Pred(sl.Count)];
      outPath := OptOut + dest;
      ForceDirectories(ExtractFilePath(outPath));
      bs := TBytesStream.Create(ResourceOpenData(container, src));
      try
        bs.SaveToFile(outPath);
      finally
        bs.Free;
      end;
      o := Manifest.A['files'].AddObject;
      o.S['path'] := dest;
      if not SameText(dest, src) then
        o.S['source'] := src;
      if ExtractFileName(container) = '' then
        o.S['container'] := 'loose'
      else
        o.S['container'] := ExtractFileName(container);
      o.S['md5'] := wbMD5File(outPath);
      AddProviders(src, o.A['providers']);
    end;
  finally
    sl.Free;
  end;
end;

// -------------------------------------------------------------------------

function ReadOptions: Boolean;
var
  opt: TJsonObject;
  path, s: string;
  interactive: Boolean;
  answer: Integer;
begin
  Result := False;
  OptPlugin := 'Atelier NPC.esp';
  OptPrefix := '';
  OptOut := '';
  OptMO2 := '';
  OptNewNpc := False;
  interactive := True;
  path := ScriptsPath + OptionsFileName;
  if FileExists(path) then begin
    opt := TJsonObject.Create;
    try
      opt.LoadFromFile(path);
      if opt.S['plugin'] <> '' then OptPlugin := opt.S['plugin'];
      OptPrefix := opt.S['prefix'];
      OptOut := opt.S['outputFolder'];
      OptMO2 := opt.S['mo2Folder'];
      if opt.Contains('newNpc') then OptNewNpc := opt.B['newNpc'];
      if opt.Contains('interactive') then interactive := opt.B['interactive'];
    finally
      opt.Free;
    end;
  end;

  if interactive then begin
    if not InputQuery('Atelier - Build NPC Mod', 'Plugin (a new ESL-flagged .esp, or a loaded plugin to add to):', OptPlugin) then
      Exit;
  end;
  if ExtractFileExt(OptPlugin) = '' then
    OptPlugin := OptPlugin + '.esp';
  if OptPrefix = '' then
    OptPrefix := DefaultPrefix(OptPlugin);
  if interactive then begin
    if not InputQuery('Atelier - Build NPC Mod', 'EditorID prefix for the copies (letters, digits, _):', OptPrefix) then
      Exit;
  end;
  if not ValidPrefix(OptPrefix) then begin
    AddMessage('The prefix may only hold letters, digits and _: ' + OptPrefix);
    Exit;
  end;

  if OptOut = '' then begin
    s := ExtractFilePath(NoSlash(ExtractFilePath(NoSlash(DataPath))));
    if FileExists(s + 'ModOrganizer.ini') then
      OptOut := s + 'mods\' + ChangeFileExt(OptPlugin, '')
    else
      OptOut := ScriptsPath + 'Atelier builds\' + ChangeFileExt(OptPlugin, '');
  end;
  if interactive then begin
    if not InputQuery('Atelier - Build NPC Mod', 'Output folder (the new mod''s folder; it is created if missing):', OptOut) then
      Exit;
  end;
  OptOut := AddSlash(Trim(OptOut));

  if interactive then begin
    answer := MessageDlg('Make a NEW NPC from each selected NPC?' + #13#10#13#10 +
      'Yes: a new NPC record in ' + OptPlugin + ' gets the face.' + #13#10 +
      'No: the selected NPC gets the face (as an override in ' + OptPlugin + ' when it is defined elsewhere).',
      mtConfirmation, [mbYes, mbNo, mbCancel], 0);
    if answer = mrCancel then
      Exit;
    OptNewNpc := answer = mrYes;
  end;
  Result := True;
end;

function Initialize: Integer;
begin
  Result := 0;
  Aborted := True;
  RecMap := TStringList.Create;
  EdidMap := TStringList.Create;
  PathMap := TStringList.Create;
  Assets := TStringList.Create;
  OwnSeen := TStringList.Create;
  RecordsLog := TStringList.Create;
  NpcLog := TStringList.Create;
  GameFiles := NewSet;
  Missing := NewSet;
  Notes := TStringList.Create;
  Manifest := TJsonObject.Create;

  if not ReadOptions then begin
    AddMessage('Atelier - Build NPC Mod: cancelled');
    Result := 1;
    Exit;
  end;

  TargetFile := FindFileByName(OptPlugin);
  TargetIsNew := not Assigned(TargetFile);
  if TargetIsNew then
    TargetFile := AddNewFileName(OptPlugin, True);
  if not Assigned(TargetFile) then begin
    AddMessage('Atelier - Build NPC Mod: could not create ' + OptPlugin);
    Result := 1;
    Exit;
  end;
  ForceDirectories(OptOut);
  LoadModList;
  Aborted := False;

  Manifest.S['format'] := 'RaceMenuAtelier.BuildNPCMod';
  Manifest.I['version'] := 1;
  Manifest.S['plugin'] := OptPlugin;
  Manifest.S['prefix'] := OptPrefix;
  Manifest.S['built'] := FormatDateTime('yyyy-mm-dd hh:nn:ss', Now);
  AddMessage('Atelier - Build NPC Mod: ' + OptPlugin + ' (new: ' + YesNo(TargetIsNew) + '), prefix ' + OptPrefix +
    ', new NPCs: ' + YesNo(OptNewNpc) + ', output ' + OptOut);
end;

function Process(e: IInterface): Integer;
var
  doc, data, npcLogObj: TJsonObject;
  parts: TJsonArray;
  part, txst, npc: IInterface;
  i: Integer;
  error: string;
begin
  Result := 0;
  if Aborted or (Signature(e) <> 'NPC_') then
    Exit;
  AddMessage(Name(e));
  error := '';
  doc := LoadFaceJson(e, error);
  if not Assigned(doc) then begin
    AddMessage('  ' + error);
    NpcLog.Add(Name(e) + ': ' + error);
    Exit;
  end;
  try
    data := doc.O['record'];
    parts := data.A['headParts'];
    for i := 0 to Pred(parts.Count) do begin
      part := FormFromJson(parts.O[i], 'HDPT');
      if Assigned(part) then
        CopyRecord(part);
    end;
    if not data.IsNull('faceTextureSet') then begin
      txst := FormFromJson(data.O['faceTextureSet'], 'TXST');
      if Assigned(txst) then
        CopyRecord(txst);
    end;

    npc := TargetNpc(e);
    if not ApplyFace(npc, doc, RecMap) then begin
      NpcLog.Add(Name(e) + ': refused, see the messages above');
      Exit;
    end;
    npcLogObj := Manifest.A['npcs'].AddObject;
    npcLogObj.S['from'] := Name(e);
    npcLogObj.S['npc'] := Name(npc);
    CopyFaceGen(doc, npc, npcLogObj);
    NpcLog.Add(Name(e) + ' -> ' + Name(npc));
    AddMessage('  face written to ' + Name(npc));
  finally
    doc.Free;
  end;
end;

function Finalize: Integer;
var
  fs: TFileStream;
  i: Integer;
  mpath: string;
begin
  Result := 0;
  if Aborted then
    Exit;
  try
    BuildMorphs;
    CopyAssets;

    for i := 0 to Pred(RecordsLog.Count) do
      Manifest.A['records'].Add(RecordsLog[i]);
    for i := 0 to Pred(GameFiles.Count) do
      Manifest.A['baseGameFiles'].Add(GameFiles[i]);
    for i := 0 to Pred(Missing.Count) do
      Manifest.A['missing'].Add(Missing[i]);
    for i := 0 to Pred(Notes.Count) do
      Manifest.A['notes'].Add(Notes[i]);
    mpath := OptOut + 'SKSE\Plugins\RaceMenuAtelier\Builds\' + ChangeFileExt(OptPlugin, '') + '.json';
    ForceDirectories(ExtractFilePath(mpath));
    Manifest.SaveToFile(mpath, False);

    CleanMasters(TargetFile);
    if TargetIsNew then begin
      fs := TFileStream.Create(OptOut + OptPlugin, fmCreate);
      try
        FileWriteToStream(TargetFile, fs, False);
      finally
        fs.Free;
      end;
      // xEdit put an empty header of the new plugin in Data (with MO2: in
      // Overwrite), which would hide the finished plugin
      if FileExists(DataPath + OptPlugin) then
        DeleteFile(DataPath + OptPlugin);
    end;

    AddMessage('');
    AddMessage('Atelier - Build NPC Mod: done');
    for i := 0 to Pred(NpcLog.Count) do
      AddMessage('  ' + NpcLog[i]);
    AddMessage(Format('  %d records copied or reused, %d files copied, %d base-game files left out, %d missing',
      [RecordsLog.Count, Manifest.A['files'].Count, GameFiles.Count, Missing.Count]));
    for i := 0 to Pred(Missing.Count) do
      AddMessage('  MISSING ' + Missing[i]);
    for i := 0 to Pred(Notes.Count) do
      AddMessage('  note: ' + Notes[i]);
    AddMessage('  manifest: ' + mpath);
    if TargetIsNew then begin
      AddMessage('  plugin written: ' + OptOut + OptPlugin);
      AddMessage('  When xEdit asks to save on exit, untick ' + OptPlugin + ': it is already saved in the output folder.');
    end else
      AddMessage('  Save ' + OptPlugin + ' when xEdit asks on exit.');
  finally
    RecMap.Free;
    EdidMap.Free;
    PathMap.Free;
    Assets.Free;
    OwnSeen.Free;
    RecordsLog.Free;
    NpcLog.Free;
    GameFiles.Free;
    Missing.Free;
    Notes.Free;
    ModList.Free;
    Manifest.Free;
  end;
end;

end.
