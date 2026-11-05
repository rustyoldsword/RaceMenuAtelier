# Assemble RaceMenu Atelier: dist\RaceMenu Atelier\ (Data layout) and a .7z ready for a mod manager.
# Run after: xmake build RaceMenuAtelier
#
# Run: powershell -ExecutionPolicy Bypass -File tools\package.ps1 [-NoArchive]

param([switch]$NoArchive)

$ErrorActionPreference = 'Stop'

$root    = Split-Path -Parent $PSScriptRoot
$version = '1.1.0'
$stage   = Join-Path $root 'dist\RaceMenu Atelier'
$archive = Join-Path $root "dist\RaceMenu Atelier $version.7z"
$dll     = Join-Path $root 'build\windows\x64\releasedbg\RaceMenuAtelier.dll'

$newest = Get-ChildItem (Join-Path $root 'src') -Recurse -File | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if ((Get-Item $dll).LastWriteTime -lt $newest.LastWriteTime) { throw "RaceMenuAtelier.dll is older than $($newest.Name) - run xmake build first" }

if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force "$stage\SKSE\Plugins", "$stage\docs" | Out-Null
Copy-Item $dll "$stage\SKSE\Plugins\"
Copy-Item (Join-Path $root 'dist_template\SKSE\Plugins\RaceMenuAtelier.ini') "$stage\SKSE\Plugins\"
Copy-Item (Join-Path $root 'dist_template\docs\RaceMenu Atelier__README.txt') "$stage\docs\"
# icons: tools\slice_icons.py writes 49 pictures x 5 sizes
Copy-Item (Join-Path $root 'dist_template\Interface') $stage -Recurse
$icons = Get-ChildItem "$stage\Interface\RaceMenuAtelier\icons" -File -Filter *.png
if ($icons.Count -ne 245) { throw "expected 245 icon files, staged $($icons.Count) - run python tools\slice_icons.py" }

$files = Get-ChildItem $stage -Recurse -File
if ($files.Count -ne 248) { throw "expected 248 files, staged $($files.Count)" }
$files | Where-Object { $_.Extension -ne '.png' } | Select-Object @{ n = 'file'; e = { $_.FullName.Substring($stage.Length) } }, Length | Format-Table -AutoSize | Out-String -Width 200
"plus $($icons.Count) icons"

if ($NoArchive) { return }
$sevenZip = @('C:\Program Files\7-Zip\7z.exe', 'C:\Program Files (x86)\7-Zip\7z.exe') | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $sevenZip) { Write-Host "7-Zip not found - staged folder only: $stage"; return }
if (Test-Path $archive) { Remove-Item $archive -Force }
Push-Location $stage
try { & $sevenZip a -t7z -mx=5 $archive * | Select-Object -Last 2 } finally { Pop-Location }
Get-Item $archive | Select-Object FullName, Length
