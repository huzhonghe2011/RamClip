param(
    [ValidateSet("all", "x64", "arm64")]
    [string]$Arch = "all"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$Root = $PSScriptRoot
$Source = Join-Path $Root "RamClip.cpp"
$Dist = Join-Path $Root "dist"

if (-not (Test-Path $Source)) {
    throw "Source file not found: $Source"
}

# dist is release output only; clear it to avoid stale packages/hashes.
if (Test-Path $Dist) {
    Remove-Item -Recurse -Force $Dist
}
New-Item -ItemType Directory -Force $Dist | Out-Null

$CommonArgs = @(
    "-std=c++20",
    "-O2",
    "-DNDEBUG",
    "-municode",
    "-mwindows",
    "-mguard=cf",
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

function Get-ImportedDllNames {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BinaryPath,

        [Parameter(Mandatory = $true)]
        [string]$Objdump
    )

    $Output = & $Objdump -p $BinaryPath 2>&1

    if ($LASTEXITCODE -ne 0) {
        throw "llvm-objdump failed for: $BinaryPath"
    }

    $Names = foreach ($Line in $Output) {
        $Text = [string]$Line

        if ($Text -match 'DLL Name:\s*(.+?)\s*$') {
            $Matches[1].Trim()
        }
    }

    return @($Names | Sort-Object -Unique)
}

function Copy-TargetRuntimeDependencies {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BinaryPath,

        [Parameter(Mandatory = $true)]
        [string]$RuntimeDir,

        [Parameter(Mandatory = $true)]
        [string]$Objdump,

        [Parameter(Mandatory = $true)]
        [string]$PackageDir
    )

    # Only DLLs found in <llvm-mingw>\<target-triple>\bin are bundled.
    # Windows system DLLs/UCRT are intentionally not copied.
    $Seen = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase
    )

    $Bundled = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase
    )

    $Queue = [System.Collections.Generic.Queue[string]]::new()
    $Queue.Enqueue($BinaryPath)

    while ($Queue.Count -gt 0) {
        $Current = $Queue.Dequeue()
        $Resolved = (Resolve-Path $Current).Path

        if (-not $Seen.Add($Resolved)) {
            continue
        }

        $Imports = Get-ImportedDllNames `
            -BinaryPath $Resolved `
            -Objdump $Objdump

        foreach ($DllName in $Imports) {
            $Candidate = Join-Path $RuntimeDir $DllName

            if (-not (Test-Path $Candidate -PathType Leaf)) {
                continue
            }

            $Destination = Join-Path $PackageDir $DllName

            if (-not (Test-Path $Destination -PathType Leaf)) {
                Copy-Item `
                    -Path $Candidate `
                    -Destination $Destination
            }

            [void]$Bundled.Add($DllName)
            $Queue.Enqueue($Destination)
        }
    }

    return @($Bundled | Sort-Object)
}

function Build-RamClip {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Compiler,

        [Parameter(Mandatory = $true)]
        [string]$Target,

        [Parameter(Mandatory = $true)]
        [string]$TargetTriple
    )

    $CompilerCommand = Get-Command `
        $Compiler `
        -ErrorAction SilentlyContinue

    if (-not $CompilerCommand) {
        throw "Compiler not found: $Compiler"
    }

    $CompilerPath = $CompilerCommand.Source
    $ToolchainBin = Split-Path -Parent $CompilerPath
    $ToolchainRoot = Split-Path -Parent $ToolchainBin
    $RuntimeDir = Join-Path `
        $ToolchainRoot `
        "$TargetTriple\bin"

    $Objdump = Join-Path $ToolchainBin "llvm-objdump.exe"

    if (-not (Test-Path $Objdump -PathType Leaf)) {
        throw "llvm-objdump.exe not found: $Objdump"
    }

    if (-not (Test-Path $RuntimeDir -PathType Container)) {
        throw "Target runtime directory not found: $RuntimeDir"
    }

    $PackageDir = Join-Path $Dist "RamClip-win-$Target"
    New-Item `
        -ItemType Directory `
        -Force `
        $PackageDir |
        Out-Null

    $OutputExe = Join-Path $PackageDir "RamClip.exe"

    Write-Host ""
    Write-Host "Building RamClip for $Target..."
    Write-Host "Compiler: $CompilerPath"
    Write-Host "Target runtime: $RuntimeDir"

    # Deliberately NOT using -static.
    # LLVM-MinGW C++ runtime DLLs will be copied beside the EXE below.
    $CompilerArgs = $CommonArgs + @(
        "-o",
        $OutputExe
    ) + $Libraries

    & $Compiler @CompilerArgs

    if ($LASTEXITCODE -ne 0) {
        throw "Build failed: $Target"
    }

    Write-Host ""
    Write-Host "Direct imports for ${Target}:"
    Get-ImportedDllNames `
        -BinaryPath $OutputExe `
        -Objdump $Objdump |
        ForEach-Object {
            Write-Host "  $_"
        }

    $RuntimeDlls = @(
        Copy-TargetRuntimeDependencies `
            -BinaryPath $OutputExe `
            -RuntimeDir $RuntimeDir `
            -Objdump $Objdump `
            -PackageDir $PackageDir
    )

    Write-Host ""
    if ($RuntimeDlls.Count -gt 0) {
        Write-Host "Bundled LLVM-MinGW runtime DLLs:"
        $RuntimeDlls |
            ForEach-Object {
                Write-Host "  $_"
            }
    } else {
        Write-Warning (
            "No target runtime DLLs were bundled. " +
            "Check the EXE imports before publishing."
        )
    }

    $ZipPath = Join-Path `
        $Dist `
        "RamClip-win-$Target-portable.zip"

    if (Test-Path $ZipPath) {
        Remove-Item -Force $ZipPath
    }

    Compress-Archive `
        -Path (Join-Path $PackageDir "*") `
        -DestinationPath $ZipPath `
        -CompressionLevel Optimal

    Write-Host ""
    Write-Host "Created: $ZipPath"
}

if ($Arch -eq "all" -or $Arch -eq "x64") {
    Build-RamClip `
        -Compiler "x86_64-w64-mingw32-clang++.exe" `
        -Target "x64" `
        -TargetTriple "x86_64-w64-mingw32"
}

if ($Arch -eq "all" -or $Arch -eq "arm64") {
    Build-RamClip `
        -Compiler "aarch64-w64-mingw32-clang++.exe" `
        -Target "arm64" `
        -TargetTriple "aarch64-w64-mingw32"
}

Write-Host ""
Write-Host "Generating SHA256SUMS.txt..."

$ZipFiles = @(
    Get-ChildItem `
        -Path $Dist `
        -Filter "*.zip" `
        -File |
        Sort-Object Name
)

if ($ZipFiles.Count -eq 0) {
    throw "No release ZIP files were generated."
}

$ZipFiles |
    ForEach-Object {
        $Hash = (
            Get-FileHash `
                $_.FullName `
                -Algorithm SHA256
        ).Hash.ToLowerInvariant()

        "$Hash  $($_.Name)"
    } |
    Set-Content `
        -Encoding ascii `
        (Join-Path $Dist "SHA256SUMS.txt")

Write-Host ""
Write-Host "Build completed:"
Get-ChildItem $Dist -Recurse
