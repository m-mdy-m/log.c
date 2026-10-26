[CmdletBinding()]
param([string]$Prefix = (Join-Path $env:LOCALAPPDATA 'm-mdy-m-log'))

$ErrorActionPreference = 'Stop'
$Root = $PSScriptRoot
$RequiredFiles = @(
    (Join-Path $Root 'include\m-mdy-m\log.h'),
    (Join-Path $Root 'lib\libm-mdy-m-log.a'),
    (Join-Path $Root 'lib\pkgconfig\m-mdy-m-log.pc')
)
foreach ($File in $RequiredFiles) {
    if (-not (Test-Path -LiteralPath $File -PathType Leaf)) {
        throw "Release archive is incomplete; missing: $File"
    }
}
$Prefix = [System.IO.Path]::GetFullPath($Prefix)
foreach ($Directory in @('include', 'lib', 'bin', 'share')) {
    $Source = Join-Path $Root $Directory
    if (-not (Test-Path -LiteralPath $Source -PathType Container)) { continue }
    $Destination = Join-Path $Prefix $Directory
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    Get-ChildItem -LiteralPath $Source -Force | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $Destination -Recurse -Force
    }
}
Write-Host "Installed m-mdy-m/log.c to $Prefix"
Write-Host "Header: $(Join-Path $Prefix 'include\m-mdy-m\log.h')"
Write-Host "Static library: $(Join-Path $Prefix 'lib\libm-mdy-m-log.a')"
Write-Host 'This Windows archive uses the MinGW-w64 UCRT64 ABI; MSVC ABI compatibility is not promised.'
