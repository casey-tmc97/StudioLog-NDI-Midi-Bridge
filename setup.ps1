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
