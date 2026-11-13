param([ValidateSet('Debug','Release')][string]$Configuration='Release', [switch]$CoreTests, [switch]$Clean)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -all -products '*' -property installationPath | Select-Object -First 1
if (!$vs) { throw 'Visual Studio C++ Build Tools not found.' }
$setup = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$environment = & cmd.exe /c "call `"$setup`" >nul && set"
if ($LASTEXITCODE -ne 0) { throw 'Cannot initialize the MSVC environment.' }
foreach($line in $environment) {
    if($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1],$matches[2],'Process') }
}
$cmake = Join-Path $root '.tools\cmake\data\bin\cmake.exe'
$ninja = Join-Path $root '.tools\ninja\data\bin\ninja.exe'
if(!(Test-Path $cmake)) { $cmake = (Get-Command cmake -ErrorAction Stop).Source }
if(!(Test-Path $ninja)) { $ninja = if(Test-Path (Join-Path $root '.tools')) { Get-ChildItem (Join-Path $root '.tools') -Filter ninja.exe -Recurse | Select-Object -First 1 -ExpandProperty FullName } }
if(!$ninja) { $ninja = (Get-Command ninja -ErrorAction Stop).Source }
& $cmake -S . -B build -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_BUILD_TYPE=$Configuration" "-DDUALSENSEZ_CORE_TESTS=$($CoreTests.IsPresent)" -DBUILD_TESTING=OFF
if($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
if($Clean) { & $cmake --build build --target clean; if($LASTEXITCODE -ne 0){throw 'Clean failed.'} }
& $cmake --build build --parallel 4 2>&1 | Tee-Object -FilePath build-output.log | Select-String -Pattern '^\[|error |fatal |FAILED:|ninja:'
if($LASTEXITCODE -ne 0) { throw 'Build failed.' }
Write-Host "Built: $root\build\DualsenseCore.lib"
