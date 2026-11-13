param([string]$Version)
# Collects the built app into dist\DualsenseZ-<version>-win-x64 and zips it for a GitHub release.
# Run scripts\build-winui.ps1 first.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$output = Join-Path $root 'winui\bin\x64\Release'
if (!(Test-Path (Join-Path $output 'DualsenseZ.exe'))) { throw 'DualsenseZ.exe not found. Run scripts\build-winui.ps1 first.' }
if (!$Version) { $Version = (Get-Content (Join-Path $root 'resources\translations\en.json') -Raw | ConvertFrom-Json).'WinUI.SettingsAppVersionValue' }
$name = "DualsenseZ-$Version-win-x64"
$dist = Join-Path $root 'dist'
$stage = Join-Path $dist $name
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force $stage | Out-Null
# Debug symbols, linker leftovers, logs and the test-only string map stay out of the package.
robocopy $output $stage /E /XF *.pdb *.ilk *.exp *.lib *.log *.dsy winui-map.json /NFL /NDL /NJH /NJS /NP | Out-Null
if ($LASTEXITCODE -ge 8) { throw "Copy failed (robocopy $LASTEXITCODE)." }
Copy-Item (Join-Path $root 'LICENSE'), (Join-Path $root 'THIRD-PARTY-NOTICES.md') -Destination $stage
$zip = Join-Path $dist "$name.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path $stage -DestinationPath $zip
$size = (Get-ChildItem $stage -Recurse -File | Measure-Object Length -Sum).Sum
Write-Host ("Packaged: {0} ({1:N0} MB unpacked, {2:N0} MB zip)" -f $zip, ($size / 1MB), ((Get-Item $zip).Length / 1MB))
