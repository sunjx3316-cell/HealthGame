param(
    [string]$QtRoot = 'C:\Qt\6.11.0\mingw_64',
    [string]$BuildDirectory = 'build-3d',
    [string]$PackageName = 'HealthGame-Growth-v0.7.1'
)
$ErrorActionPreference = 'Stop'
$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$buildRoot = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $BuildDirectory))
if (-not $buildRoot.StartsWith($repoRoot + [System.IO.Path]::DirectorySeparatorChar)) {
    throw 'Build directory must be inside this repository.'
}
if ($PackageName -notmatch '^HealthGame-[A-Za-z0-9._-]+$') { throw 'Invalid package directory name.' }
$packageRoot = Join-Path (Join-Path $repoRoot 'dist') $PackageName
$deploy = Join-Path $QtRoot 'bin\windeployqt.exe'
$binary = Join-Path $buildRoot 'HealthGame.exe'
if (-not (Test-Path -LiteralPath $binary)) { throw 'Build HealthGame first.' }
New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null
Copy-Item -LiteralPath $binary -Destination (Join-Path $packageRoot 'HealthGame.exe') -Force
# Assets come from source, not a stale build tree.
Copy-Item -LiteralPath (Join-Path $repoRoot '第四版\assets\anatomy') -Destination $packageRoot -Recurse -Force
Copy-Item -LiteralPath (Join-Path $repoRoot 'ANATOMY-3D.md') -Destination $packageRoot -Force
Copy-Item -LiteralPath (Join-Path $repoRoot 'BALANCE.md') -Destination $packageRoot -Force
Copy-Item -LiteralPath (Join-Path $repoRoot 'THIRD_PARTY_NOTICES.md') -Destination $packageRoot -Force
Copy-Item -LiteralPath (Join-Path $repoRoot 'licenses') -Destination $packageRoot -Recurse -Force
$previousPath = $env:PATH
try {
    $env:PATH = (Join-Path $QtRoot 'bin') + ';C:\Qt\Tools\mingw1310_64\bin;' + $env:PATH
    & $deploy --release --compiler-runtime --no-translations --dir $packageRoot (Join-Path $packageRoot 'HealthGame.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Qt deployment failed.' }
} finally { $env:PATH = $previousPath }
Write-Output $packageRoot
