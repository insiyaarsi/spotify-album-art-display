# Build Wokwi firmware with arduino-cli.
# Usage: .\scripts\build_wokwi.ps1
# Optional: $env:ARDUINO_CLI = "path\to\arduino-cli.exe"

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Wokwi = Join-Path $Root "wokwi"
$Cli = if ($env:ARDUINO_CLI) { $env:ARDUINO_CLI } else { "arduino-cli" }

if (-not (Get-Command $Cli -ErrorAction SilentlyContinue)) {
    $LocalCli = Join-Path $Root "tools\arduino-cli.exe"
    if (Test-Path $LocalCli) { $Cli = $LocalCli }
    else {
        Write-Error "arduino-cli not found. Install it, set ARDUINO_CLI, or run from a dev container."
    }
}

Write-Host "Installing ESP32 core (if needed)..."
& $Cli core update-index
& $Cli core install esp32:esp32

Write-Host "Installing libraries (if needed)..."
Get-Content (Join-Path $Wokwi "libraries.txt") | ForEach-Object {
    $lib = $_.Trim()
    if ($lib -and -not $lib.StartsWith("#")) {
        & $Cli lib install $lib
    }
}

New-Item -ItemType Directory -Force -Path (Join-Path $Wokwi "build") | Out-Null
Write-Host "Compiling wokwi.ino (clean build so TFT_eSPI picks up tft_setup.h)..."
& $Cli compile --clean --fqbn esp32:esp32:esp32 --output-dir (Join-Path $Wokwi "build") (Join-Path $Wokwi "wokwi.ino")

$Bin = Join-Path $Wokwi "build\wokwi.ino.bin"
if (-not (Test-Path $Bin)) {
    Write-Error "Build finished but firmware binary is missing at $Bin"
}

Write-Host "Firmware ready: $Bin"
