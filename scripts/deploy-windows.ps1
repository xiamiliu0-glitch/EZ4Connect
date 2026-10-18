# Windows deployment script
# Usage: .\deploy-windows.ps1 -TargetName "EZ4Connect" -DisplayName "EZ4Connect" -BuildDir "build" -Architecture "amd64" -Nightly "false"

param(
    [string]$TargetName = "EZ4Connect",
    [string]$DisplayName = "EZ4Connect",
    [string]$BuildDir = "build",
    [string]$Architecture = "amd64",
    [string]$Nightly = "false",
    [string]$ZjuConnectVersion = ""
)

Import-Module -Name Microsoft.PowerShell.Utility

# Create output directory
New-Item -ItemType Directory -Path "$DisplayName" -Force
Push-Location "$DisplayName"

# Copy executable
Copy-Item -Path "../$BuildDir/Release/$TargetName.exe" -Destination .

# Run windeployqt
& windeployqt.exe --release --compiler-runtime "$TargetName.exe"
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed with exit code $LASTEXITCODE"
}

# The hosted runner does not set VCINSTALLDIR, so windeployqt can leave out the
# MSVC runtime. Copy the redistributable CRT beside the executable for a portable ZIP.
if ($Architecture -eq "amd64") {
    $VsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio/Installer/vswhere.exe"
    if (-not (Test-Path $VsWhere)) { throw "vswhere.exe not found" }
    $VsPath = & $VsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $VsPath) { throw "Visual Studio C++ tools not found" }
    $CrtRoot = Join-Path $VsPath "VC/Redist/MSVC"
    $CrtDirectory = Get-ChildItem $CrtRoot -Directory |
        Sort-Object Name -Descending |
        ForEach-Object { Join-Path $_.FullName "x64/Microsoft.VC143.CRT" } |
        Where-Object { Test-Path $_ } |
        Select-Object -First 1
    if (-not $CrtDirectory) { throw "MSVC x64 redistributable runtime not found" }
    Copy-Item (Join-Path $CrtDirectory "*.dll") -Destination .
    foreach ($dll in @("msvcp140.dll", "vcruntime140.dll", "vcruntime140_1.dll")) {
        if (-not (Test-Path $dll)) { throw "Missing MSVC runtime: $dll" }
    }

    # This optional NMEA plugin cannot load without Qt6SerialPort.dll on the runner.
    Remove-Item "position/qtposition_nmea.dll" -ErrorAction SilentlyContinue
}

# Download and extract zju-connect
if ($ZjuConnectVersion -ne "" -and $ZjuConnectVersion -notmatch '^v[0-9]+\.[0-9]+\.[0-9]+$') {
    throw "Invalid zju-connect version: $ZjuConnectVersion"
}
$ZjuReleasePath = if ($ZjuConnectVersion -ne "") {
    "download/$ZjuConnectVersion"
} elseif ($Nightly -eq "true") {
    "download/nightly"
} else {
    "latest/download"
}
$ZjuUrl = "https://github.com/Mythologyli/zju-connect/releases/$ZjuReleasePath/zju-connect-windows-$Architecture.zip"
$ZjuZipPath = "zju-connect-windows-$Architecture.zip"
Invoke-WebRequest -Uri $ZjuUrl -OutFile $ZjuZipPath
Expand-Archive -Path $ZjuZipPath -DestinationPath . -Force
Remove-Item -Path $ZjuZipPath

# Copy additional files
Copy-Item -Path "../libs/wintun/bin/$Architecture/wintun.dll" -Destination .
Copy-Item -Path "../resource/qt.conf" -Destination .

Pop-Location
