[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Archive,
    [Parameter(Mandatory = $true)][string]$Prefix
)

$ErrorActionPreference = 'Stop'
$Archive = (Resolve-Path -LiteralPath $Archive).Path
$Prefix = [System.IO.Path]::GetFullPath($Prefix)
$ExtractRoot = Join-Path $env:TEMP "m-mdy-m-log-e2e-$PID"
if (Test-Path -LiteralPath $ExtractRoot) {
    Remove-Item -LiteralPath $ExtractRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $ExtractRoot -Force | Out-Null

try {
    Expand-Archive -LiteralPath $Archive -DestinationPath $ExtractRoot -Force
    $Bundle = Get-ChildItem -LiteralPath $ExtractRoot -Directory |
        Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'install.ps1') -PathType Leaf } |
        Select-Object -First 1
    if (-not $Bundle) {
        throw 'Release ZIP does not contain install.ps1 at its package root.'
    }

    & (Join-Path $Bundle.FullName 'install.ps1') -Prefix $Prefix

    $Required = @(
        (Join-Path $Prefix 'include\m-mdy-m\log.h'),
        (Join-Path $Prefix 'lib\libm-mdy-m-log.a'),
        (Join-Path $Prefix 'lib\pkgconfig\m-mdy-m-log.pc'),
        (Join-Path $Prefix 'lib\cmake\m-mdy-m-log\m-mdy-m-log-config.cmake')
    )
    foreach ($Path in $Required) {
        if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
            throw "PowerShell installer E2E failed; missing installed file: $Path"
        }
    }

    $DllCandidates = @(
        (Join-Path $Prefix 'bin\m-mdy-m-log.dll'),
        (Join-Path $Prefix 'bin\libm-mdy-m-log.dll')
    )
    if (-not ($DllCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf })) {
        throw 'PowerShell installer E2E failed; shared DLL was not installed.'
    }
    Write-Host 'Windows ZIP / install.ps1 E2E: PASS'
}
finally {
    Remove-Item -LiteralPath $ExtractRoot -Recurse -Force -ErrorAction SilentlyContinue
}
