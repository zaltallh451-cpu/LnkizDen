[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Destination
)

$ErrorActionPreference = "Stop"

$repository = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$payload = Join-Path $repository "build\bin\LnkizDen.elf"
$dashboardGuide = Join-Path $repository "docs\companion-dashboard.md"
$license = Join-Path $repository "LICENSE"
$exampleConfig = Join-Path $repository "config.ini.example"

foreach ($requiredFile in @($payload, $dashboardGuide, $license, $exampleConfig)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Required release input is missing: $requiredFile"
    }
}

$destinationPath = [System.IO.Path]::GetFullPath($Destination)
if (Test-Path -LiteralPath $destinationPath) {
    throw "Destination already exists; choose a new empty staging path: $destinationPath"
}

$parent = Split-Path -Parent $destinationPath
if (-not (Test-Path -LiteralPath $parent -PathType Container)) {
    throw "Destination parent must already exist: $parent"
}

New-Item -ItemType Directory -Path $destinationPath | Out-Null
Copy-Item -LiteralPath $payload -Destination (Join-Path $destinationPath "LnkizDen.elf")
Copy-Item -LiteralPath $dashboardGuide -Destination (Join-Path $destinationPath "companion-dashboard.md")
Copy-Item -LiteralPath $license -Destination (Join-Path $destinationPath "LICENSE")
Copy-Item -LiteralPath $exampleConfig -Destination (Join-Path $destinationPath "config.ini.example")

$hash = (Get-FileHash -LiteralPath (Join-Path $destinationPath "LnkizDen.elf") -Algorithm SHA256).Hash.ToLowerInvariant()
@"
LnkizDen Companion USB bundle

Payload: LnkizDen.elf
SHA256:  $hash

Copy this directory to a USB drive that you have already formatted as exFAT.
This script does not format drives or modify existing files.
Follow companion-dashboard.md to enable and pair the LAN dashboard.
"@ | Set-Content -LiteralPath (Join-Path $destinationPath "README.txt") -Encoding ASCII

Write-Output "Staged exFAT-ready bundle at: $destinationPath"
Write-Output "The destination is a directory for copying to an already formatted USB drive; no drive was formatted."
