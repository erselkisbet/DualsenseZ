param([switch]$SkipCore, [switch]$SkipRestore, [switch]$Clean)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Set-Location $root
if(!$SkipCore) { & "$PSScriptRoot\build.ps1" -Configuration Release -CoreTests -Clean:$Clean }
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs=& $vswhere -all -products '*' -property installationPath | Select-Object -First 1
$msbuild=Join-Path $vs 'MSBuild\Current\Bin\MSBuild.exe'
if(!$SkipRestore) {
    & $msbuild winui/DualsenseZ.WinUI.vcxproj /t:Restore /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo
    if($LASTEXITCODE -ne 0){throw 'WinUI NuGet restore failed.'}
}
$target=if($Clean){'Rebuild'}else{'Build'}
& $msbuild winui/DualsenseZ.WinUI.vcxproj /t:$target /p:Configuration=Release /p:Platform=x64 /m:2 /v:minimal /nologo /fl /flp:logfile=build-winui.log
if($LASTEXITCODE -ne 0){throw 'WinUI build failed.'}
$output=Join-Path $root 'winui\bin\x64\Release'
if(Test-Path "$root\build\hidapi.dll") { Copy-Item -LiteralPath "$root\build\hidapi.dll" -Destination $output -Force }
Copy-Item -LiteralPath "$root\resources" -Destination $output -Recurse -Force
Write-Host "Built: $output\DualsenseZ.exe"
