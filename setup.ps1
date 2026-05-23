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
