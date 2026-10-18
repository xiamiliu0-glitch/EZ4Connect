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
