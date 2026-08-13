param(
    [ValidateSet("all", "x64", "arm64")]
    [string]$Arch = "all"
)

$ErrorActionPreference = "Stop"

$Root = $PSScriptRoot
$Source = Join-Path $Root "RamClip.cpp"
$Dist = Join-Path $Root "dist"

if (-not (Test-Path $Source)) {
    throw "Source file not found: $Source"
}

New-Item -ItemType Directory -Force $Dist | Out-Null

$CommonArgs = @(
    "-std=c++20",
    "-O2",
    "-s",
    "-municode",
    "-mwindows",
    "-Wl,--no-insert-timestamp",
    $Source
)

$Libraries = @(
    "-ld2d1",
    "-ldwrite",
    "-lgdi32",
    "-luser32",
    "-lshell32"
)

function Build-RamClip {
    param(
        [string]$Compiler,
        [string]$Target
    )

    $CompilerPath = Get-Command $Compiler -ErrorAction SilentlyContinue

    if (-not $CompilerPath) {
        throw "Compiler not found: $Compiler"
    }

    Write-Host ""
    Write-Host "Building RamClip for $Target..."
    Write-Host "Compiler: $($CompilerPath.Source)"

    # Normal build
    $Output = Join-Path $Dist "RamClip-win-$Target.exe"

    $Args = $CommonArgs + @(
        "-o",
        $Output
    ) + $Libraries

    & $Compiler @Args

    if ($LASTEXITCODE -ne 0) {
        throw "Build failed: $Target"
    }

    # Standalone / static runtime build
    $StandaloneOutput =
        Join-Path $Dist "RamClip-win-$Target-standalone.exe"

    $StandaloneArgs = $CommonArgs + @(
        "-static",
        "-o",
        $StandaloneOutput
    ) + $Libraries

    & $Compiler @StandaloneArgs

    if ($LASTEXITCODE -ne 0) {
        throw "Standalone build failed: $Target"
    }
}

if ($Arch -eq "all" -or $Arch -eq "x64") {
    Build-RamClip `
        "x86_64-w64-mingw32-clang++.exe" `
        "x64"
}

if ($Arch -eq "all" -or $Arch -eq "arm64") {
    Build-RamClip `
        "aarch64-w64-mingw32-clang++.exe" `
        "arm64"
}

Write-Host ""
Write-Host "Generating SHA256SUMS.txt..."

Get-ChildItem (Join-Path $Dist "*.exe") |
    Sort-Object Name |
    ForEach-Object {
        $Hash = (
            Get-FileHash $_.FullName -Algorithm SHA256
        ).Hash.ToLowerInvariant()

        "$Hash  $($_.Name)"
    } |
    Set-Content `
        -Encoding ascii `
        (Join-Path $Dist "SHA256SUMS.txt")

Write-Host ""
Write-Host "Build completed:"
Get-ChildItem $Dist
