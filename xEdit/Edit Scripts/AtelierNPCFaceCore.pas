{
  AtelierNPCFaceCore - shared code of the RaceMenu Atelier xEdit scripts
  ("Atelier - Apply NPC Appearance" and "Atelier - Build NPC Mod").

  Reads the face RaceMenu Atelier ("FaceGen for NPC") exported for an NPC:
    SKSE\Plugins\RaceMenuAtelier\NPCFaces\<plugin>\<id>.json
  and writes it into an NPC_ record: PNAM head parts, HCLF hair colour,
  FTST head texture, QNAM texture lighting, NAM9 face morphs, NAMA face parts,
  tint layers (TINI/TINC/TINV/TIAS), NAM6 height and NAM7 weight.
}
unit AtelierNPCFaceCore;

const
  FacesFolder = 'SKSE\Plugins\RaceMenuAtelier\NPCFaces\';
  FaceGeomFolder = 'meshes\actors\character\FaceGenData\FaceGeom\';
  FaceTintFolder = 'textures\actors\character\FaceGenData\FaceTint\';
  HairColorPrefix = 'RMA_HairColor_';

function FindFileByName(aName: string): IInterface;
var
  i: Integer;
  f: IInterface;
begin
  Result := nil;
  for i := 0 to Pred(FileCount) do begin
    f := FileByIndex(i);
    if SameText(GetFileName(f), aName) then begin
      Result := f;
      Exit;
    end;
  end;
end;

// a record from {"plugin", "formID", "editorID"} as the json names it
function FormFromJson(aObj: TJsonObject; aSig: string): IInterface;
var
  f, r: IInterface;
  local: Integer;
begin
  Result := nil;
  if aObj.S['plugin'] = '' then
    Exit;
  f := FindFileByName(aObj.S['plugin']);
  if not Assigned(f) then
    Exit;
  if aObj.S['formID'] <> '' then begin
    local := StrToInt('$' + aObj.S['formID']);
    r := RecordByFormID(f, (MasterCount(f) shl 24) or local, False);
    if Assigned(r) then
      if Signature(r) = aSig then begin
        Result := r;
        Exit;
      end;
  end;
  if aObj.S['editorID'] <> '' then begin
    r := MainRecordByEditorID(GroupBySignature(f, aSig), aObj.S['editorID']);
    if Assigned(r) then
      Result := r;
  end;
end;

function DescribeJson(aObj: TJsonObject): string;
begin
  Result := aObj.S['editorID'] + ' [' + aObj.S['plugin'] + '|' + aObj.S['formID'] + ']';
end;

// can a record in aTargetFile point at aRec?
function Reachable(aTargetFile, aRec: IInterface): Boolean;
var
  rf: IInterface;
begin
  rf := GetFile(MasterOrSelf(aRec));
  Result := Equals(rf, aTargetFile) or HasMaster(aTargetFile, GetFileName(rf));
end;

function LoadOrderHex(aRec: IInterface): string;
begin
  Result := IntToHex(GetLoadOrderFormID(aRec), 8);
end;

// the NPC's local FormID as FaceGen names it (12 bits for a light plugin)
function LocalHex(aNpc: IInterface): string;
var
  m: IInterface;
  local: Integer;
begin
  m := MasterOrSelf(aNpc);
  local := FormID(m) and $FFFFFF;
  if GetIsESL(GetFile(m)) then
    local := local and $FFF;
  Result := IntToHex(local, 8);
end;

// the face json for an NPC, or nil (aError says why)
function LoadFaceJson(aNpc: IInterface; var aError: string): TJsonObject;
var
  plugin, hex, path: string;
  doc: TJsonObject;
begin
  Result := nil;
  plugin := GetFileName(GetFile(MasterOrSelf(aNpc)));
  hex := LocalHex(aNpc);
  path := DataPath + FacesFolder + plugin + '\' + hex + '.json';
  if not FileExists(path) then begin
    aError := 'no exported face: ' + path;
    Exit;
  end;
  doc := TJsonObject.Create;
  doc.LoadFromFile(path);
  if doc.S['format'] <> 'RaceMenuAtelier.NPCFace' then begin
    aError := path + ' is not a RaceMenu Atelier face';
    doc.Free;
    Exit;
  end;
  if (not SameText(doc.O['npc'].S['plugin'], plugin)) or (doc.O['npc'].S['formID'] <> hex) then begin
    aError := 'the json was written for ' + doc.O['npc'].S['plugin'] + '|' + doc.O['npc'].S['formID'];
    doc.Free;
    Exit;
  end;
  Result := doc;
end;

// -------------------------------------------------------------------------
// writers

procedure WriteHeadParts(e: IInterface; aWanted: TStringList);
var
  hp, entry: IInterface;
  current: TStringList;
  i: Integer;
begin
  current := TStringList.Create;
  try
    current.Sorted := True;
    hp := ElementByName(e, 'Head Parts');
    if Assigned(hp) then
      for i := 0 to Pred(ElementCount(hp)) do
        current.Add(IntToHex(GetLoadOrderFormID(LinksTo(ElementByIndex(hp, i))), 8));
    if current.CommaText = aWanted.CommaText then
      Exit;
  finally
    current.Free;
  end;

  RemoveElement(e, 'Head Parts');
  if aWanted.Count = 0 then
    Exit;
  hp := Add(e, 'Head Parts', True);
  for i := 0 to Pred(aWanted.Count) do begin
    if i = 0 then
      entry := ElementByIndex(hp, 0)
    else
      entry := ElementAssign(hp, HighInteger, nil, False);
    SetEditValue(entry, aWanted[i]);
  end;
end;

function HairColorRecord(aTargetFile: IInterface; aEditorID: string; r, g, b: Integer): IInterface;
var
  grp, rec: IInterface;
begin
  grp := GroupBySignature(aTargetFile, 'CLFM');
  if not Assigned(grp) then
    grp := Add(aTargetFile, 'CLFM', True);
  rec := MainRecordByEditorID(grp, aEditorID);
  if not Assigned(rec) then begin
    rec := Add(grp, 'CLFM', True);
    SetEditorID(rec, aEditorID);
    AddMessage('    created CLFM ' + aEditorID);
  end;
  Add(rec, 'CNAM', True);
  SetElementNativeValues(rec, 'CNAM\Red', r);
  SetElementNativeValues(rec, 'CNAM\Green', g);
  SetElementNativeValues(rec, 'CNAM\Blue', b);
  SetElementNativeValues(rec, 'CNAM\Alpha', 0);
  Add(rec, 'FNAM', True);
  SetElementNativeValues(rec, 'FNAM', 0);
  Result := rec;
end;

procedure WriteUnsigned(aElement: IInterface; aValue: Integer);
begin
  if aValue < 0 then
    SetEditValue(aElement, '4294967295')
  else
    SetNativeValue(aElement, aValue);
end;

procedure WriteTintLayers(e: IInterface; aLayers: TJsonArray);
var
  tl, layer: IInterface;
  i: Integer;
  o: TJsonObject;
begin
  RemoveElement(e, 'Tint Layers');
  if aLayers.Count = 0 then
    Exit;
  tl := Add(e, 'Tint Layers', True);
  for i := 0 to Pred(aLayers.Count) do begin
    o := aLayers.O[i];
    if i = 0 then
      layer := ElementByIndex(tl, 0)
    else
      layer := ElementAssign(tl, HighInteger, nil, False);
    Add(layer, 'TINI', True);
    Add(layer, 'TINC', True);
    Add(layer, 'TINV', True);
    Add(layer, 'TIAS', True);
    SetElementNativeValues(layer, 'TINI', o.I['index']);
    SetElementNativeValues(layer, 'TINC\Red', o.A['color'].I[0]);
    SetElementNativeValues(layer, 'TINC\Green', o.A['color'].I[1]);
    SetElementNativeValues(layer, 'TINC\Blue', o.A['color'].I[2]);
    SetElementNativeValues(layer, 'TINC\Alpha', 0);
    SetElementNativeValues(layer, 'TINV', Round(o.F['value'] * 100));
    SetElementNativeValues(layer, 'TIAS', o.I['preset']);
  end;
end;

// the record to point at for a json form: its copy when aMap names one
// (Values[original load-order hex] = copy's load-order hex), else itself
// when the target plugin can reach it, else '' (aProblem says why)
function TargetHex(tf, aRec: IInterface; aMap: TStringList; var aProblem: string): string;
begin
  Result := '';
  if Assigned(aMap) then
    Result := aMap.Values[LoadOrderHex(MasterOrSelf(aRec))];
  if Result <> '' then
    Exit;
  if Reachable(tf, aRec) then
    Result := LoadOrderHex(aRec)
  else
    aProblem := EditorID(aRec) + ' is in ' + GetFileName(GetFile(MasterOrSelf(aRec))) + ', which is not a master of ' + GetFileName(tf);
end;

// -------------------------------------------------------------------------

// Writes the face of aDoc into the NPC record e. aMap (may be nil) redirects
// head parts and the head texture set to copies (see TargetHex).
function ApplyFace(e: IInterface; aDoc: TJsonObject; aMap: TStringList): Boolean;
var
  tf, race, part, hair, txst, nam9, nama: IInterface;
  data: TJsonObject;
  parts: TJsonArray;
  morphs, faceParts: TJsonArray;
  o, hc: TJsonObject;
  wanted: TStringList;
  i: Integer;
  problems, edid, hex, problem: string;
  female: Boolean;
begin
  Result := False;
  tf := GetFile(e);
  data := aDoc.O['record'];
  problems := '';

  // race and sex must match the face
  race := FormFromJson(data.O['race'], 'RACE');
  if not Assigned(race) then
    problems := problems + #13#10 + '    race ' + DescribeJson(data.O['race']) + ' not found'
  else if GetLoadOrderFormID(LinksTo(ElementBySignature(e, 'RNAM'))) <> GetLoadOrderFormID(race) then
    problems := problems + #13#10 + '    the record''s race is ' + GetElementEditValues(e, 'RNAM') + ', the face was made on ' + EditorID(race);
  female := (GetElementNativeValues(e, 'ACBS\Flags') and 1) <> 0;
  if female <> data.B['female'] then
    problems := problems + #13#10 + '    sex differs from the exported face';

  // head parts: all must resolve and be reachable from this plugin
  wanted := TStringList.Create;
  try
    wanted.Sorted := True;
    wanted.Duplicates := dupIgnore;
    parts := data.A['headParts'];
    for i := 0 to Pred(parts.Count) do begin
      o := parts.O[i];
      part := FormFromJson(o, 'HDPT');
      if not Assigned(part) then
        problems := problems + #13#10 + '    head part ' + DescribeJson(o) + ' not found (created at runtime or plugin not loaded)'
      else begin
        problem := '';
        hex := TargetHex(tf, part, aMap, problem);
        if hex = '' then
          problems := problems + #13#10 + '    head part ' + problem
        else
          wanted.Add(hex);
      end;
    end;

    if problems <> '' then begin
      AddMessage('  REFUSED ' + Name(e) + ':' + problems);
      Exit;
    end;

    if aDoc.A['warnings'].Count > 0 then
      for i := 0 to Pred(aDoc.A['warnings'].Count) do
        AddMessage('  note from the export: ' + aDoc.A['warnings'].S[i]);

    // PNAM
    WriteHeadParts(e, wanted);
  finally
    wanted.Free;
  end;

  // HCLF
  if data.IsNull('hairColor') then
    RemoveElement(e, 'HCLF')
  else begin
    hc := data.O['hairColor'];
    hair := nil;
    if not hc.IsNull('form') then begin
      hair := FormFromJson(hc.O['form'], 'CLFM');
      if Assigned(hair) then
        if not Reachable(tf, hair) then
          hair := nil;
    end;
    if not Assigned(hair) then begin
      edid := HairColorPrefix + Copy(LocalHex(e), 3, 6);
      hair := HairColorRecord(tf, edid, hc.A['rgb'].I[0], hc.A['rgb'].I[1], hc.A['rgb'].I[2]);
    end;
    Add(e, 'HCLF', True);
    SetElementEditValues(e, 'HCLF', LoadOrderHex(hair));
  end;

  // FTST
  if data.IsNull('faceTextureSet') then
    RemoveElement(e, 'FTST')
  else begin
    txst := FormFromJson(data.O['faceTextureSet'], 'TXST');
    hex := '';
    problem := '';
    if Assigned(txst) then
      hex := TargetHex(tf, txst, aMap, problem);
    if hex <> '' then begin
      Add(e, 'FTST', True);
      SetElementEditValues(e, 'FTST', hex);
    end else
      AddMessage('  head texture ' + DescribeJson(data.O['faceTextureSet']) + ' is not reachable from ' + GetFileName(tf) + '; FTST left unchanged');
  end;

  // QNAM: stored as floats 0..1, edited in xEdit as 0..255
  Add(e, 'QNAM', True);
  SetElementEditValues(e, 'QNAM\Red', IntToStr(data.A['skinTone'].I[0]));
  SetElementEditValues(e, 'QNAM\Green', IntToStr(data.A['skinTone'].I[1]));
  SetElementEditValues(e, 'QNAM\Blue', IntToStr(data.A['skinTone'].I[2]));

  // NAM9
  if not data.IsNull('morphs') then begin
    morphs := data.A['morphs'];
    Add(e, 'NAM9', True);
    nam9 := ElementBySignature(e, 'NAM9');
    for i := 0 to Pred(ElementCount(nam9)) do
      if i < morphs.Count then
        SetNativeValue(ElementByIndex(nam9, i), morphs.F[i]);
  end;

  // NAMA
  if not data.IsNull('faceParts') then begin
    faceParts := data.A['faceParts'];
    Add(e, 'NAMA', True);
    nama := ElementBySignature(e, 'NAMA');
    WriteUnsigned(ElementByIndex(nama, 0), faceParts.I[0]);
    SetNativeValue(ElementByIndex(nama, 1), faceParts.I[1]);
    WriteUnsigned(ElementByIndex(nama, 2), faceParts.I[2]);
    WriteUnsigned(ElementByIndex(nama, 3), faceParts.I[3]);
  end;

  // tint layers
  WriteTintLayers(e, data.A['tintLayers']);

  // NAM6 / NAM7
  SetElementNativeValues(e, 'NAM6', data.F['height']);
  SetElementNativeValues(e, 'NAM7', data.F['weight']);

  Result := True;
end;

end.
