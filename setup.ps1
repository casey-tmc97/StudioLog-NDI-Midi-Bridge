#Requires -Version 7.0
<#
.SYNOPSIS
  First-time setup and build for StudioLog NDI/MIDI Bridge on Windows.
.PARAMETER Stage
  Which stage to run: all | prereqs | ndi | build  (default: all)
.PARAMETER Skip
  Items to skip in prereqs: cmake, vs, qt, vcpkg, loopmidi  (comma-separated)
.PARAMETER Config
  CMake configuration: Debug | Release  (default: Debug)
.PARAMETER Qt6Dir
  Override auto-detected Qt6 cmake dir.
.PARAMETER VcpkgRoot
  Override auto-detected vcpkg root.
#>
[CmdletBinding()]
param(
    [ValidateSet('all','prereqs','ndi','build')]
    [string]$Stage = 'all',

    [string[]]$Skip = @(),

    [ValidateSet('Debug','Release')]
    [string]$Config = 'Debug',

    [string]$Qt6Dir    = '',
    [string]$VcpkgRoot = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# ─── Output helpers ───────────────────────────────────────────────────────────
function Write-Ok   ([string]$msg) { Write-Host "  ✅  $msg" -ForegroundColor Green  }
function Write-Skip ([string]$msg) { Write-Host "  ⏭️  $msg" -ForegroundColor Yellow }
function Write-Info ([string]$msg) { Write-Host "  ℹ️  $msg" -ForegroundColor Cyan   }
function Write-Fail ([string]$msg) { Write-Host "  ❌  $msg" -ForegroundColor Red    }
function Write-Head ([string]$msg) {
    $bar = '─' * [Math]::Max(1, 52 - $msg.Length)
    Write-Host "`n─── $msg $bar" -ForegroundColor White
}

# ─── Global state ─────────────────────────────────────────────────────────────
$script:Failures = [System.Collections.Generic.List[string]]::new()
$script:Summary  = [ordered]@{}
function Add-Failure ([string]$msg) { $script:Failures.Add($msg) }

# ─── VS detection ─────────────────────────────────────────────────────────────
function Get-VSInfo {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { return $null }

    $installPath = & $vswhere -latest -products * `
        -requires Microsoft.VisualCpp.Tools.HostX64.TargetX64 `
        -property installationPath 2>$null
    $version = & $vswhere -latest -products * `
        -requires Microsoft.VisualCpp.Tools.HostX64.TargetX64 `
        -property installationVersion 2>$null

    if (-not $installPath -or -not $version) { return $null }

    $major = [int]($version -split '\.')[0]
    $generator = switch ($major) {
        17      { "Visual Studio 17 2022" }
        18      { "Visual Studio 18 2025" }
        default { $null }
    }

    return [PSCustomObject]@{
        InstallPath = $installPath.Trim()
        Version     = $version.Trim()
        Major       = $major
        Generator   = $generator
    }
}

# ─── CMakePresets.json patching ───────────────────────────────────────────────
function Invoke-PatchCMakePresets ([string]$Generator) {
    $presetsFile = Join-Path $PSScriptRoot "CMakePresets.json"
    $content     = Get-Content $presetsFile -Raw

    $pattern     = '"generator":\s*"Visual Studio \d+ \d+"'
    $replacement = "`"generator`": `"$Generator`""
    $patched     = [regex]::Replace($content, $pattern, $replacement)

    if ($patched -eq $content) {
        Write-Skip "CMakePresets.json already uses: $Generator"
        return
    }

    Set-Content -Path $presetsFile -Value $patched -Encoding UTF8 -NoNewline
    Write-Ok "Patched CMakePresets.json → $Generator"

    $dirty = git -C $PSScriptRoot status --porcelain CMakePresets.json 2>$null
    if ($dirty) {
        git -C $PSScriptRoot add CMakePresets.json
        git -C $PSScriptRoot commit -m "chore: sync CMakePresets.json generator to installed VS ($Generator)"
        Write-Ok "Committed CMakePresets.json update"
    }
}

# ─── CMake PATH ───────────────────────────────────────────────────────────────
function Add-CMakeToPath ([string]$VSInstallPath) {
    if (Get-Command cmake -ErrorAction SilentlyContinue) { return $null }

    $cmakeBin = Join-Path $VSInstallPath `
        "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"

    if (-not (Test-Path (Join-Path $cmakeBin "cmake.exe"))) { return $null }

    $env:PATH = "$cmakeBin;$env:PATH"
    return $cmakeBin
}

# ─── Qt 6 ─────────────────────────────────────────────────────────────────────
$script:QT_VERSION = '6.8.3'
$script:QT_ARCH    = 'win64_msvc2022_64'
$script:QT_ROOT    = 'C:\Qt'

function Test-Qt6 {
    # 1. -Qt6Dir override
    if ($Qt6Dir -and (Test-Path (Join-Path $Qt6Dir "Qt6Config.cmake"))) {
        return $Qt6Dir
    }
    # 2. User env var
    $envDir = [System.Environment]::GetEnvironmentVariable('Qt6_DIR', 'User')
    if ($envDir -and (Test-Path (Join-Path $envDir "Qt6Config.cmake"))) {
        return $envDir
    }
    # 3. Known default path (arch arg matches disk dir name)
    $default = "$script:QT_ROOT\$script:QT_VERSION\$script:QT_ARCH\lib\cmake\Qt6"
    if (Test-Path (Join-Path $default "Qt6Config.cmake")) { return $default }
    # 4. Scan under QT_ROOT\QT_VERSION\ — aqt may shorten the arch dir name
    $versionDir = "$script:QT_ROOT\$script:QT_VERSION"
    if (Test-Path $versionDir) {
        $found = Get-ChildItem -Path $versionDir -Directory |
            ForEach-Object { Join-Path $_.FullName "lib\cmake\Qt6" } |
            Where-Object   { Test-Path (Join-Path $_ "Qt6Config.cmake") } |
            Select-Object  -First 1
        if ($found) { return $found }
    }
    return $null
}

function Install-Qt6 {
    $found = Test-Qt6
    if ($found) {
        $env:Qt6_DIR = $found
        Write-Skip "Qt $script:QT_VERSION already at $found"
        $script:Summary['Qt 6'] = "✅  $script:QT_VERSION  $found"
        return
    }

    Write-Info "Installing Qt $script:QT_VERSION via aqtinstall (this takes a few minutes)…"

    # Ensure aqtinstall is available
    if (-not (Get-Command aqt -ErrorAction SilentlyContinue)) {
        pip install aqtinstall --quiet 2>&1 | Write-Host
        if ($LASTEXITCODE -ne 0) {
            Add-Failure "pip install aqtinstall failed. Ensure Python/pip is on PATH."
            $script:Summary['Qt 6'] = "❌  aqtinstall unavailable"
            return
        }
    }

    python -m aqt install-qt windows desktop $script:QT_VERSION $script:QT_ARCH `
        -O $script:QT_ROOT 2>&1 | Write-Host

    if ($LASTEXITCODE -ne 0) {
        Add-Failure "Qt install failed. Run manually: python -m aqt install-qt windows desktop $script:QT_VERSION $script:QT_ARCH -O $script:QT_ROOT"
        $script:Summary['Qt 6'] = "❌  install failed"
        return
    }

    # Discover the actual installed cmake dir (aqt may shorten the arch dir name)
    $qt6Dir = Test-Qt6
    if (-not $qt6Dir) {
        Add-Failure "Qt install reported success but Qt6Config.cmake not found under $script:QT_ROOT\$script:QT_VERSION"
        $script:Summary['Qt 6'] = "❌  cmake dir not found after install"
        return
    }

    [System.Environment]::SetEnvironmentVariable('Qt6_DIR', $qt6Dir, 'User')
    $env:Qt6_DIR = $qt6Dir
    Write-Ok "Qt $script:QT_VERSION installed → $qt6Dir"
    $script:Summary['Qt 6'] = "✅  $script:QT_VERSION  $qt6Dir"
}

# ─── Stage placeholders (replaced in later tasks) ─────────────────────────────
function Invoke-StagePrereqs { Write-Info "prereqs stage — not yet implemented" }
function Invoke-StageNdi     { Write-Info "ndi stage — not yet implemented"     }
function Invoke-StageBuild   { Write-Info "build stage — not yet implemented"   }
function Show-Summary        { Write-Info "summary — not yet implemented"        }

# ─── Main ─────────────────────────────────────────────────────────────────────
function Main {
    Write-Host ""
    Write-Host "╔══════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║   StudioLog NDI/MIDI Bridge — Windows Setup      ║" -ForegroundColor Cyan
    Write-Host "╚══════════════════════════════════════════════════╝" -ForegroundColor Cyan

    switch ($Stage) {
        'prereqs' { Invoke-StagePrereqs }
        'ndi'     { Invoke-StageNdi     }
        'build'   { Invoke-StageBuild   }
        'all'     {
            Invoke-StagePrereqs
            Invoke-StageNdi
            if ($script:Failures.Count -eq 0) {
                Invoke-StageBuild
            } else {
                Write-Fail "Skipping build — fix failures above first, then re-run .\setup.ps1"
            }
        }
    }

    Show-Summary
}

# Guard: only run Main when executed directly, not dot-sourced
if ($MyInvocation.InvocationName -ne '.') { Main }
