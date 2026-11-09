{
  Atelier - Apply NPC Appearance

  Applies a face exported by RaceMenu Atelier ("FaceGen for NPC") to the
  selected NPC records.

  RaceMenu Atelier writes, for an NPC defined in <plugin> with local FormID <id>:
    meshes\actors\character\FaceGenData\FaceGeom\<plugin>\<id>.nif
    textures\actors\character\FaceGenData\FaceTint\<plugin>\<id>.dds
    SKSE\Plugins\RaceMenuAtelier\NPCFaces\<plugin>\<id>.json

  Select one or more NPC_ records (the original or an override) and apply this
  script. For each record it reads the matching .json through the Data folder
  (under MO2 that includes Overwrite) and writes into that record:
    PNAM head parts, HCLF hair colour (creates a CLFM when the colour is not a
    form reachable from the plugin), FTST head texture, QNAM texture lighting,
    NAM9 face morphs, NAMA face parts, tint layers (TINI/TINC/TINV/TIAS),
    NAM6 height and NAM7 weight.

  Safety:
    - nothing is written to a record whose race or sex differs from the
      exported face (the FaceGen head would not match its head parts);
    - every head part must be defined in the record's plugin or one of its
      masters, otherwise the record is refused and the part is named;
    - running it again with the same .json gives the same record.

  Needs AtelierNPCFaceCore.pas in the same folder.
}
unit AtelierApplyNPCAppearance;

uses AtelierNPCFaceCore;

var
  CountApplied, CountRefused, CountMissing: Integer;

function Initialize: Integer;
begin
  CountApplied := 0;
  CountRefused := 0;
  CountMissing := 0;
  AddMessage('Atelier - Apply NPC Appearance: faces from ' + DataPath + FacesFolder);
  Result := 0;
end;

function Process(e: IInterface): Integer;
var
  plugin, hex, nifPath, error: string;
  doc: TJsonObject;
begin
  Result := 0;
  if Signature(e) <> 'NPC_' then
    Exit;

  plugin := GetFileName(GetFile(MasterOrSelf(e)));
  hex := LocalHex(e);
  AddMessage(Name(e) + ' -> ' + plugin + '\' + hex);
  error := '';
  doc := LoadFaceJson(e, error);
  if not Assigned(doc) then begin
    AddMessage('  ' + error);
    if Pos('no exported face', error) = 1 then
      Inc(CountMissing)
    else
      Inc(CountRefused);
    Exit;
  end;

  try
    if ApplyFace(e, doc, nil) then begin
      Inc(CountApplied);
      AddMessage('  applied: ' + IntToStr(doc.O['record'].A['headParts'].Count) + ' head parts, ' +
        IntToStr(doc.O['record'].A['tintLayers'].Count) + ' tint layers, height ' + FloatToStr(doc.O['record'].F['height']));
      nifPath := DataPath + FaceGeomFolder + plugin + '\' + hex + '.nif';
      if not FileExists(nifPath) then
        AddMessage('  WARNING: no FaceGen head at ' + nifPath + ' - the face will be dark in game');
    end else
      Inc(CountRefused);
  finally
    doc.Free;
  end;
end;

function Finalize: Integer;
begin
  AddMessage(Format('Atelier - Apply NPC Appearance: %d applied, %d refused, %d without an exported face', [CountApplied, CountRefused, CountMissing]));
  Result := 0;
end;

end.
