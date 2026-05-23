# Setup Script Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create `setup.ps1` — a single idempotent PowerShell script that installs all Windows build prerequisites, validates the NDI SDK, configures CMake, and builds the project.

**Architecture:** Ten tasks build up `setup.ps1` function by function — skeleton first, then one helper per prerequisite, then three stage orchestrators, and a final summary table. Each task replaces a named placeholder function with a real implementation, so the script is always runnable. The spec is at `docs/superpowers/specs/2026-05-23-setup-script-design.md`.

**Tech Stack:** PowerShell 7, winget, aqtinstall (Python), git, vcpkg, CMake 4.2.3 (VS-bundled), Visual Studio 18 2025 BuildTools

---

## File Structure

| File | Action | Responsibility |
|---|---|---|
| `setup.ps1` | **Create** | All setup logic — one file, ~350 lines |
| `CMakePresets.json` | **Modify at runtime** | Generator string auto-patched by the script |

---

### Task 1 — Script skeleton, parameters, and output helpers

**Files:**
- Create: `setup.ps1`

- [ ] **Step 1: Create `setup.ps1` with the full skeleton**

```powershell
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
```

- [ ] **Step 2: Verify the skeleton runs without errors**

```powershell
cd C:\Users\Admin\Documents\GitHub\StudioLog-NDI-Midi-Bridge
.\setup.ps1
```

Expected output (truncated):
```
╔══════════════════════════════════════════════════╗
║   StudioLog NDI/MIDI Bridge — Windows Setup      ║
╚══════════════════════════════════════════════════╝

─── Stage: prereqs ─ (etc.)
  ℹ️  prereqs stage — not yet implemented
  ℹ️  ndi stage — not yet implemented
  ℹ️  build stage — not yet implemented
  ℹ️  summary — not yet implemented
```
No red errors, exit code 0.

- [ ] **Step 3: Test `-Stage` routing**

```powershell
.\setup.ps1 -Stage prereqs
```

Expected: banner + `ℹ️ prereqs stage — not yet implemented` + `ℹ️ summary — not yet implemented`. No other stage output.

- [ ] **Step 4: Commit**

```powershell
git add setup.ps1
git commit -m "feat: setup.ps1 skeleton with parameters and output helpers"
```

---

### Task 2 — VS detection and CMakePresets.json patching

**Files:**
- Modify: `setup.ps1` (add `Get-VSInfo` and `Invoke-PatchCMakePresets` before `Main`)
- Modify: `CMakePresets.json` (patched at runtime by `Invoke-PatchCMakePresets`)

- [ ] **Step 1: Add `Get-VSInfo` and `Invoke-PatchCMakePresets` to `setup.ps1`**

Insert these two functions immediately before the `# ─── Stage placeholders` comment block:

```powershell
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
```

- [ ] **Step 2: Test `Get-VSInfo` via dot-source**

```powershell
. .\setup.ps1   # dot-source loads functions without running Main
Get-VSInfo
```

Expected:
```
InstallPath : C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools
Version     : 18.x.x.x
Major       : 18
Generator   : Visual Studio 18 2025
```

- [ ] **Step 3: Verify `CMakePresets.json` currently has VS 17**

```powershell
Select-String '"generator"' CMakePresets.json
```

Expected: three lines each containing `"Visual Studio 17 2022"`.

- [ ] **Step 4: Test `Invoke-PatchCMakePresets`**

```powershell
. .\setup.ps1
Invoke-PatchCMakePresets "Visual Studio 18 2025"
Select-String '"generator"' CMakePresets.json
```

Expected: `✅ Patched CMakePresets.json → Visual Studio 18 2025`, git commit, then three lines each containing `"Visual Studio 18 2025"`.

- [ ] **Step 5: Commit**

```powershell
git add setup.ps1
git commit -m "feat(setup): VS detection and CMakePresets.json auto-patching"
```

---

### Task 3 — CMake PATH helper

**Files:**
- Modify: `setup.ps1` (add `Add-CMakeToPath` after `Invoke-PatchCMakePresets`)

- [ ] **Step 1: Add `Add-CMakeToPath` to `setup.ps1`**

Insert after `Invoke-PatchCMakePresets`:

```powershell
# ─── CMake PATH ───────────────────────────────────────────────────────────────
function Add-CMakeToPath ([string]$VSInstallPath) {
    if (Get-Command cmake -ErrorAction SilentlyContinue) { return $null }

    $cmakeBin = Join-Path $VSInstallPath `
        "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"

    if (-not (Test-Path (Join-Path $cmakeBin "cmake.exe"))) { return $null }

    $env:PATH = "$cmakeBin;$env:PATH"
    return $cmakeBin
}
```

- [ ] **Step 2: Test via dot-source**

```powershell
. .\setup.ps1
$vs  = Get-VSInfo
$bin = Add-CMakeToPath $vs.InstallPath
Write-Host "Added: $bin"
cmake --version
```

Expected:
```
Added: C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\...CMake\bin
cmake version 4.2.3-msvc3
```

- [ ] **Step 3: Commit**

```powershell
git add setup.ps1
git commit -m "feat(setup): VS-bundled CMake path helper"
```

---

### Task 4 — Qt 6 installer

Uses Python's `aqtinstall` (Python 3.14 is already on this machine at `C:\Python314\`). Installs Qt 6.8.3 MSVC x64 to `C:\Qt` and sets the `Qt6_DIR` user environment variable.

**Files:**
- Modify: `setup.ps1` (add Qt constants + `Test-Qt6` + `Install-Qt6` after `Add-CMakeToPath`)

- [ ] **Step 1: Add Qt installer functions**

Insert after `Add-CMakeToPath`:

```powershell
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
    # 3. Known default path
    $default = "$script:QT_ROOT\$script:QT_VERSION\$script:QT_ARCH\lib\cmake\Qt6"
    if (Test-Path (Join-Path $default "Qt6Config.cmake")) { return $default }
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

    python -m aqt install-qt Windows Desktop $script:QT_VERSION $script:QT_ARCH `
        -O $script:QT_ROOT 2>&1 | Write-Host

    if ($LASTEXITCODE -ne 0) {
        Add-Failure "Qt install failed. Run manually: python -m aqt install-qt Windows Desktop $script:QT_VERSION $script:QT_ARCH -O $script:QT_ROOT"
        $script:Summary['Qt 6'] = "❌  install failed"
        return
    }

    $qt6Dir = "$script:QT_ROOT\$script:QT_VERSION\$script:QT_ARCH\lib\cmake\Qt6"
    [System.Environment]::SetEnvironmentVariable('Qt6_DIR', $qt6Dir, 'User')
    $env:Qt6_DIR = $qt6Dir
    Write-Ok "Qt $script:QT_VERSION installed → $qt6Dir"
    $script:Summary['Qt 6'] = "✅  $script:QT_VERSION  $qt6Dir"
}
```

- [ ] **Step 2: Verify `Test-Qt6` returns `$null` before install**

```powershell
. .\setup.ps1
Test-Qt6
```

Expected: empty output (returns `$null`).

- [ ] **Step 3: Run `Install-Qt6` (downloads ~1 GB — takes 3–10 min)**

```powershell
. .\setup.ps1
Install-Qt6
```

Expected: aqtinstall download progress, then:
```
  ✅  Qt 6.8.3 installed → C:\Qt\6.8.3\win64_msvc2022_64\lib\cmake\Qt6
```

- [ ] **Step 4: Verify `Test-Qt6` now returns the path**

```powershell
. .\setup.ps1
Test-Qt6
```

Expected: `C:\Qt\6.8.3\win64_msvc2022_64\lib\cmake\Qt6`

- [ ] **Step 5: Commit**

```powershell
git add setup.ps1
git commit -m "feat(setup): Qt 6 install via aqtinstall"
```

---

### Task 5 — vcpkg installer + rtmidi

Clones vcpkg to `$HOME\vcpkg`, bootstraps it, sets `VCPKG_ROOT`, and installs `rtmidi:x64-windows` (needed by `find_package(rtmidi CONFIG REQUIRED)` in classic vcpkg mode — no `vcpkg.json` manifest exists).

**Files:**
- Modify: `setup.ps1` (add `Test-Vcpkg` + `Install-Vcpkg` + `Install-VcpkgPackages` after `Install-Qt6`)

- [ ] **Step 1: Add vcpkg installer functions**

Insert after `Install-Qt6`:

```powershell
# ─── vcpkg ────────────────────────────────────────────────────────────────────
function Test-Vcpkg {
    # 1. -VcpkgRoot override
    if ($VcpkgRoot -and (Test-Path (Join-Path $VcpkgRoot "vcpkg.exe"))) {
        return $VcpkgRoot
    }
    # 2. User env var
    $envRoot = [System.Environment]::GetEnvironmentVariable('VCPKG_ROOT', 'User')
    if ($envRoot -and (Test-Path (Join-Path $envRoot "vcpkg.exe"))) {
        return $envRoot
    }
    # 3. Known default path
    $default = Join-Path $env:USERPROFILE "vcpkg"
    if (Test-Path (Join-Path $default "vcpkg.exe")) { return $default }
    return $null
}

function Install-VcpkgPackages ([string]$VcpkgPath) {
    $vcpkgExe = Join-Path $VcpkgPath "vcpkg.exe"
    $installed = & $vcpkgExe list 2>$null | Select-String 'rtmidi'
    if ($installed) {
        Write-Skip "rtmidi:x64-windows already in vcpkg"
        return
    }
    Write-Info "Installing rtmidi:x64-windows via vcpkg…"
    & $vcpkgExe install rtmidi:x64-windows 2>&1 | Write-Host
    if ($LASTEXITCODE -ne 0) {
        Add-Failure "vcpkg install rtmidi:x64-windows failed — see output above."
    } else {
        Write-Ok "rtmidi:x64-windows installed"
    }
}

function Install-Vcpkg {
    $found = Test-Vcpkg
    if ($found) {
        $env:VCPKG_ROOT = $found
        Write-Skip "vcpkg already at $found"
        $script:Summary['vcpkg'] = "✅  $found"
        Install-VcpkgPackages $found
        return
    }

    $vcpkgRoot = Join-Path $env:USERPROFILE "vcpkg"
    Write-Info "Cloning vcpkg to $vcpkgRoot…"
    git clone https://github.com/microsoft/vcpkg $vcpkgRoot 2>&1 | Write-Host

    Write-Info "Bootstrapping vcpkg…"
    & "$vcpkgRoot\bootstrap-vcpkg.bat" -disableMetrics 2>&1 | Write-Host

    if (-not (Test-Path (Join-Path $vcpkgRoot "vcpkg.exe"))) {
        Add-Failure "vcpkg bootstrap failed — check output above."
        $script:Summary['vcpkg'] = "❌  bootstrap failed"
        return
    }

    [System.Environment]::SetEnvironmentVariable('VCPKG_ROOT', $vcpkgRoot, 'User')
    $env:VCPKG_ROOT = $vcpkgRoot
    Write-Ok "vcpkg installed → $vcpkgRoot"
    $script:Summary['vcpkg'] = "✅  $vcpkgRoot"

    Install-VcpkgPackages $vcpkgRoot
}
```

- [ ] **Step 2: Test `Test-Vcpkg` before installation**

```powershell
. .\setup.ps1
Test-Vcpkg
```

Expected: empty output (`$null`).

- [ ] **Step 3: Run `Install-Vcpkg` (clones ~200 MB, builds vcpkg, installs rtmidi — ~5 min)**

```powershell
. .\setup.ps1
Install-Vcpkg
```

Expected: git clone output, bootstrap compilation output, then:
```
  ✅  vcpkg installed → C:\Users\Admin\vcpkg
  ✅  rtmidi:x64-windows installed
```

- [ ] **Step 4: Verify vcpkg and rtmidi**

```powershell
Test-Path "$env:USERPROFILE\vcpkg\vcpkg.exe"
& "$env:USERPROFILE\vcpkg\vcpkg.exe" list | Select-String rtmidi
```

Expected: `True`, then a line like `rtmidi:x64-windows   6.0.0   ...`

- [ ] **Step 5: Commit**

```powershell
git add setup.ps1
git commit -m "feat(setup): vcpkg clone, bootstrap, and rtmidi:x64-windows install"
```

---

### Task 6 — loopMIDI installer

Checks registry for an existing loopMIDI installation; installs via winget if absent. Non-fatal if winget fails — prints manual URL and continues.

**Files:**
- Modify: `setup.ps1` (add `Test-LoopMidi` + `Install-LoopMidi` after `Install-Vcpkg`)

- [ ] **Step 1: Add loopMIDI functions**

Insert after `Install-Vcpkg`:

```powershell
# ─── loopMIDI ─────────────────────────────────────────────────────────────────
function Test-LoopMidi {
    $regPaths = @(
        'HKCU:\Software\Tobias Erichsen\loopMIDI',
        'HKLM:\Software\Tobias Erichsen\loopMIDI',
        'HKLM:\Software\WOW6432Node\Tobias Erichsen\loopMIDI',
        'HKLM:\SYSTEM\CurrentControlSet\Services\loopMIDI'
    )
    foreach ($p in $regPaths) {
        if (Test-Path $p) { return $true }
    }
    # Also check via winget list
    $listed = winget list --id TobiasErichsen.loopMIDI 2>$null | Select-String 'loopMIDI'
    return [bool]$listed
}

function Install-LoopMidi {
    if (Test-LoopMidi) {
        Write-Skip "loopMIDI already installed"
        $script:Summary['loopMIDI'] = "✅  installed"
        return
    }

    Write-Info "Installing loopMIDI via winget…"
    winget install --id TobiasErichsen.loopMIDI --silent `
        --accept-source-agreements --accept-package-agreements 2>&1 | Write-Host

    if ($LASTEXITCODE -eq 0) {
        Write-Ok "loopMIDI installed — launch it and create a virtual MIDI port before running the app."
        $script:Summary['loopMIDI'] = "✅  installed (create a virtual port before running the app)"
    } else {
        Write-Fail "winget install loopMIDI failed — install manually."
        Write-Host "    https://www.tobias-erichsen.de/software/loopmidi.html" -ForegroundColor Yellow
        Add-Failure "loopMIDI: install manually from https://www.tobias-erichsen.de/software/loopmidi.html"
        $script:Summary['loopMIDI'] = "❌  install failed (runtime only — build continues)"
    }
}
```

- [ ] **Step 2: Test `Test-LoopMidi`**

```powershell
. .\setup.ps1
Test-LoopMidi
```

Expected: `False` if loopMIDI is not yet installed.

- [ ] **Step 3: Test `Install-LoopMidi`**

```powershell
. .\setup.ps1
Install-LoopMidi
```

Expected: either `✅ loopMIDI installed` (winget succeeded) or `❌ ... install manually` with URL (non-fatal, exits cleanly).

- [ ] **Step 4: Commit**

```powershell
git add setup.ps1
git commit -m "feat(setup): loopMIDI registry check and winget install"
```

---

### Task 7 — Prereqs stage orchestration

Replace the `Invoke-StagePrereqs` placeholder. Calls all functions from Tasks 2–6, respects `-Skip`, collects all failures before reporting them.

**Files:**
- Modify: `setup.ps1` (replace `Invoke-StagePrereqs` placeholder)

- [ ] **Step 1: Replace the `Invoke-StagePrereqs` placeholder**

Find this line:
```powershell
function Invoke-StagePrereqs { Write-Info "prereqs stage — not yet implemented" }
```

Replace it with:

```powershell
function Invoke-StagePrereqs {
    Write-Head "Stage 1: Prerequisites"

    # ── VS BuildTools + CMake ────────────────────────────────────────────────
    if ('vs' -notin $Skip -and 'cmake' -notin $Skip) {
        $vs = Get-VSInfo
        if (-not $vs) {
            Add-Failure "VS BuildTools not found. Install from: https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022"
            $script:Summary['VS BuildTools'] = "❌  not found"
            $script:Summary['CMake']         = "❌  depends on VS"
        } elseif (-not $vs.Generator) {
            Add-Failure "Unknown VS major version $($vs.Major) — update Get-VSInfo generator map in setup.ps1."
            $script:Summary['VS BuildTools'] = "❌  unknown version $($vs.Major)"
            $script:Summary['CMake']         = "❌  unknown VS version"
        } else {
            Write-Ok "VS BuildTools $($vs.Version) → generator: $($vs.Generator)"
            $script:Summary['VS BuildTools'] = "✅  $($vs.Version)  ($($vs.Generator))"
            Invoke-PatchCMakePresets $vs.Generator
            $cmakeBin = Add-CMakeToPath $vs.InstallPath
            if ($cmakeBin) { Write-Ok "CMake added to PATH from VS BuildTools" }
            $cmakeVer = (cmake --version 2>$null | Select-Object -First 1) -replace 'cmake version ', ''
            $script:Summary['CMake'] = "✅  $cmakeVer  (VS BuildTools)"
        }
    } else {
        Write-Skip "VS / CMake (in -Skip list)"
        $script:Summary['VS BuildTools'] = "⏭️  skipped"
        $script:Summary['CMake']         = "⏭️  skipped"
    }

    # ── Qt 6 ────────────────────────────────────────────────────────────────
    if ('qt' -notin $Skip) {
        Install-Qt6
    } else {
        Write-Skip "Qt 6 (in -Skip list)"
        $script:Summary['Qt 6'] = "⏭️  skipped"
    }

    # ── vcpkg ───────────────────────────────────────────────────────────────
    if ('vcpkg' -notin $Skip) {
        Install-Vcpkg
    } else {
        Write-Skip "vcpkg (in -Skip list)"
        $script:Summary['vcpkg'] = "⏭️  skipped"
    }

    # ── loopMIDI ────────────────────────────────────────────────────────────
    if ('loopmidi' -notin $Skip) {
        Install-LoopMidi
    } else {
        Write-Skip "loopMIDI (in -Skip list)"
        $script:Summary['loopMIDI'] = "⏭️  skipped"
    }

    # ── Report any failures ──────────────────────────────────────────────────
    if ($script:Failures.Count -gt 0) {
        Write-Host ""
        Write-Host "  The following items need attention before the build:" -ForegroundColor Red
        foreach ($f in $script:Failures) {
            Write-Host "    • $f" -ForegroundColor Red
        }
    }
}
```

- [ ] **Step 2: Run the prereqs stage end-to-end**

```powershell
.\setup.ps1 -Stage prereqs
```

Expected: each item shows ✅ (already installed from Tasks 2–6) or ❌ with a message. No unhandled exceptions.

- [ ] **Step 3: Test `-Skip` flag**

```powershell
.\setup.ps1 -Stage prereqs -Skip qt,loopmidi
```

Expected: Qt 6 and loopMIDI show `⏭️ skipped`; VS/CMake and vcpkg check normally.

- [ ] **Step 4: Commit**

```powershell
git add setup.ps1
git commit -m "feat(setup): prereqs stage orchestration with failure collection"
```

---

### Task 8 — NDI stage

Replace the `Invoke-StageNdi` placeholder. Opens the Vizrt browser page, waits for the user to place the SDK, then validates all three required paths.

**Files:**
- Modify: `setup.ps1` (replace `Invoke-StageNdi` placeholder)

- [ ] **Step 1: Replace the `Invoke-StageNdi` placeholder**

Find:
```powershell
function Invoke-StageNdi { Write-Info "ndi stage — not yet implemented" }
```

Replace with:

```powershell
function Invoke-StageNdi {
    Write-Head "Stage 2: NDI Advanced SDK"

    $headerFile = Join-Path $PSScriptRoot "third_party\NDI\include\Processing.NDI.Lib.h"
    $libFile    = Join-Path $PSScriptRoot "third_party\NDI\Lib\x64\Processing.NDI.Lib.x64.lib"
    $dllFile    = Join-Path $PSScriptRoot "third_party\NDI\Bin\x64\Processing.NDI.Lib.x64.dll"

    $allPresent = (Test-Path $headerFile) -and (Test-Path $libFile) -and (Test-Path $dllFile)

    if ($allPresent) {
        Write-Ok "NDI Advanced SDK found at third_party/NDI/"
        $script:Summary['NDI SDK'] = "✅  third_party/NDI/include/Processing.NDI.Lib.h"
        return
    }

    Write-Fail "NDI Advanced SDK files missing:"
    if (-not (Test-Path $headerFile)) {
        Write-Host "    third_party\NDI\include\Processing.NDI.Lib.h" -ForegroundColor Red
    }
    if (-not (Test-Path $libFile)) {
        Write-Host "    third_party\NDI\Lib\x64\Processing.NDI.Lib.x64.lib" -ForegroundColor Red
    }
    if (-not (Test-Path $dllFile)) {
        Write-Host "    third_party\NDI\Bin\x64\Processing.NDI.Lib.x64.dll" -ForegroundColor Red
    }

    Write-Host @"

  Expected layout after placing the SDK:
  third_party/NDI/
  ├── include/
  │   └── Processing.NDI.Lib.h
  ├── Lib/
  │   └── x64/
  │       └── Processing.NDI.Lib.x64.lib
  └── Bin/
      └── x64/
          └── Processing.NDI.Lib.x64.dll
"@ -ForegroundColor Gray

    Write-Info "Opening NDI SDK download page in your browser…"
    Start-Process "https://ndi.video/for-developers/ndi-sdk/"

    Write-Host ""
    Write-Host "  1. Download the 'NDI Advanced SDK for Windows' (free registration)." -ForegroundColor Yellow
    Write-Host "  2. Install / unzip it." -ForegroundColor Yellow
    Write-Host "  3. Copy the files into third_party/NDI/ as shown above." -ForegroundColor Yellow
    Write-Host ""
    Read-Host "  Press Enter once the SDK files are in place (Ctrl+C to abort)"

    # Re-validate
    $allPresent = (Test-Path $headerFile) -and (Test-Path $libFile) -and (Test-Path $dllFile)
    if (-not $allPresent) {
        Write-Fail "NDI SDK still missing after confirmation. Re-run .\setup.ps1 after placing the files."
        $script:Summary['NDI SDK'] = "❌  missing after confirmation"
        Add-Failure "NDI SDK: place files in third_party/NDI/ then re-run .\setup.ps1"
        return
    }

    Write-Ok "NDI Advanced SDK verified."
    $script:Summary['NDI SDK'] = "✅  third_party/NDI/include/Processing.NDI.Lib.h"
}
```

- [ ] **Step 2: Test the missing-SDK path**

```powershell
.\setup.ps1 -Stage ndi
```

Expected: missing-file list, layout diagram, browser opens to NDI SDK page, prompt appears. Press `Ctrl+C` to abort without placing files — confirms the wait works.

- [ ] **Step 3: Test the happy path with dummy files**

```powershell
New-Item -Force -ItemType File "third_party\NDI\include\Processing.NDI.Lib.h"   | Out-Null
New-Item -Force -ItemType File "third_party\NDI\Lib\x64\Processing.NDI.Lib.x64.lib" | Out-Null
New-Item -Force -ItemType File "third_party\NDI\Bin\x64\Processing.NDI.Lib.x64.dll" | Out-Null
.\setup.ps1 -Stage ndi
```

Expected: `✅ NDI Advanced SDK found at third_party/NDI/`

Then clean up dummy files:
```powershell
Remove-Item -Recurse "third_party\NDI" -Force
```

- [ ] **Step 4: Commit**

```powershell
git add setup.ps1
git commit -m "feat(setup): NDI SDK validation stage with browser prompt"
```

---

### Task 9 — Build stage

Replace the `Invoke-StageBuild` placeholder. Refreshes session env vars from user scope, adds Qt `bin/` to PATH, runs cmake configure then build, prints the `.exe` path on success, and exits with code 1 on failure.

**Files:**
- Modify: `setup.ps1` (replace `Invoke-StageBuild` placeholder)

- [ ] **Step 1: Replace the `Invoke-StageBuild` placeholder**

Find:
```powershell
function Invoke-StageBuild { Write-Info "build stage — not yet implemented" }
```

Replace with:

```powershell
function Invoke-StageBuild {
    Write-Head "Stage 3: Configure + Build ($Config)"

    # ── Ensure cmake is on PATH ──────────────────────────────────────────────
    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
        $vs = Get-VSInfo
        if ($vs) { Add-CMakeToPath $vs.InstallPath }
    }
    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
        Write-Fail "cmake not found — run '.\setup.ps1 -Stage prereqs' first."
        $script:Summary['Build'] = "❌  cmake not on PATH"
        return
    }

    # ── Load env vars from user scope (supports re-entry after prereqs) ──────
    if (-not $env:VCPKG_ROOT) {
        $env:VCPKG_ROOT = [System.Environment]::GetEnvironmentVariable('VCPKG_ROOT', 'User')
    }
    if (-not $env:Qt6_DIR) {
        $env:Qt6_DIR = [System.Environment]::GetEnvironmentVariable('Qt6_DIR', 'User')
    }

    if (-not $env:VCPKG_ROOT) {
        Write-Fail "VCPKG_ROOT not set — run '.\setup.ps1 -Stage prereqs' first."
        $script:Summary['Build'] = "❌  VCPKG_ROOT missing"
        return
    }
    if (-not $env:Qt6_DIR) {
        Write-Fail "Qt6_DIR not set — run '.\setup.ps1 -Stage prereqs' first."
        $script:Summary['Build'] = "❌  Qt6_DIR missing"
        return
    }

    # ── Add Qt bin/ so windeployqt is found by the CMake post-build step ────
    # Qt6_DIR = .../<version>/<arch>/lib/cmake/Qt6  →  strip 3 levels for <arch>/ root
    $qtRoot = Split-Path (Split-Path (Split-Path $env:Qt6_DIR))
    $qt6Bin = Join-Path $qtRoot "bin"
    if (Test-Path $qt6Bin) { $env:PATH = "$qt6Bin;$env:PATH" }

    # ── Select presets based on -Config ─────────────────────────────────────
    $configPreset = if ($Config -eq 'Release') { 'windows-msvc-release' } else { 'windows-msvc-debug' }
    $buildPreset  = if ($Config -eq 'Release') { 'windows-release'      } else { 'windows-debug'      }

    # ── cmake configure ──────────────────────────────────────────────────────
    Write-Info "cmake --preset $configPreset"
    $configOut = cmake --preset $configPreset 2>&1
    $configOut | Write-Host
    if ($LASTEXITCODE -ne 0) {
        Write-Fail "cmake configure failed. Last 30 lines:"
        $configOut | Select-Object -Last 30 | ForEach-Object {
            Write-Host "    $_" -ForegroundColor Red
        }
        $script:Summary['Build'] = "❌  configure failed"
        exit 1
    }
    Write-Ok "CMake configure succeeded"

    # ── cmake build ──────────────────────────────────────────────────────────
    Write-Info "cmake --build --preset $buildPreset"
    $buildOut = cmake --build --preset $buildPreset 2>&1
    $buildOut | Write-Host
    if ($LASTEXITCODE -ne 0) {
        Write-Fail "cmake build failed. Last 30 lines:"
        $buildOut | Select-Object -Last 30 | ForEach-Object {
            Write-Host "    $_" -ForegroundColor Red
        }
        $script:Summary['Build'] = "❌  build failed"
        exit 1
    }

    # ── Locate and report the exe ─────────────────────────────────────────────
    $buildDir = Join-Path $PSScriptRoot "build\windows-$($Config.ToLower())\$Config"
    $exe      = Join-Path $buildDir "StudioLogNDIMIDIBridge.exe"
    if (Test-Path $exe) {
        Write-Ok "Build succeeded → $exe"
        $script:Summary['Build'] = "✅  $exe"
    } else {
        Write-Ok "Build succeeded (exe in: $buildDir)"
        $script:Summary['Build'] = "✅  $buildDir"
    }
}
```

- [ ] **Step 2: Test the pre-prereqs failure path**

In a new shell (so env vars aren't set):

```powershell
.\setup.ps1 -Stage build
```

Expected: `❌ cmake not found — run prereqs first.` (or VCPKG_ROOT/Qt6_DIR message if cmake is already on PATH). No cmake invocation.

- [ ] **Step 3: After all prereqs + NDI SDK are in place, run a full build**

```powershell
.\setup.ps1 -Stage build
```

Expected: cmake configure output (~10 s), build output (~60 s), then:
```
  ✅  Build succeeded → C:\...\build\windows-debug\Debug\StudioLogNDIMIDIBridge.exe
```

- [ ] **Step 4: Commit**

```powershell
git add setup.ps1
git commit -m "feat(setup): cmake configure + build stage"
```

---

### Task 10 — Summary table and full end-to-end verification

Replace `Show-Summary` placeholder and verify the complete `.\setup.ps1` run.

**Files:**
- Modify: `setup.ps1` (replace `Show-Summary` placeholder)

- [ ] **Step 1: Replace the `Show-Summary` placeholder**

Find:
```powershell
function Show-Summary { Write-Info "summary — not yet implemented" }
```

Replace with:

```powershell
function Show-Summary {
    Write-Host ""
    Write-Host "╔══════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║       StudioLog NDI/MIDI Bridge — Setup Summary          ║" -ForegroundColor Cyan
    Write-Host "╠══════════════════════════════════════════════════════════╣" -ForegroundColor Cyan

    foreach ($key in $script:Summary.Keys) {
        $val   = $script:Summary[$key]
        $color = if ($val -match '^✅') { 'Green' } `
            elseif ($val -match '^❌')  { 'Red'   } `
            else                        { 'Yellow' }
        $cell = "  {0,-16} {1}" -f ($key + ':'), $val
        Write-Host ("║" + $cell.PadRight(58) + "║") -ForegroundColor $color
    }

    Write-Host "╚══════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
    Write-Host ""

    if ($script:Failures.Count -gt 0) {
        Write-Host "  ❌  $($script:Failures.Count) item(s) need attention — fix them, then re-run .\setup.ps1" `
            -ForegroundColor Red
    } else {
        Write-Host "  🎉  All checks passed." -ForegroundColor Green
    }
    Write-Host ""
}
```

- [ ] **Step 2: Run the full script end-to-end**

```powershell
.\setup.ps1
```

Expected: banner, Stage 1 (all ✅ or ⏭️), Stage 2 (✅ or interactive), Stage 3 (cmake output + ✅), then summary table with all green rows and `🎉 All checks passed.`

- [ ] **Step 3: Verify re-run safety (run immediately again)**

```powershell
.\setup.ps1
```

Expected: all items skip green within ~5 seconds. No re-installs, no cmake re-configure.

- [ ] **Step 4: Verify `-Skip` and `-Config Release`**

```powershell
.\setup.ps1 -Skip loopmidi -Config Release
```

Expected: loopMIDI row shows `⏭️ skipped`, build uses `windows-msvc-release` / `windows-release` presets.

- [ ] **Step 5: Final commit**

```powershell
git add setup.ps1
git commit -m "feat(setup): summary table and full end-to-end wiring"
```

---

## Self-Review

### Spec coverage check

| Spec requirement | Task |
|---|---|
| `-Stage`, `-Skip`, `-Config`, `-Qt6Dir`, `-VcpkgRoot` params | Task 1 |
| VS detection via `vswhere` | Task 2 |
| CMakePresets.json generator patch + git commit | Task 2 |
| CMake from VS BuildTools on PATH | Task 3 |
| Qt 6 install via aqtinstall, `Qt6_DIR` user env var | Task 4 |
| vcpkg clone + bootstrap + `VCPKG_ROOT` user env var | Task 5 |
| rtmidi:x64-windows via vcpkg | Task 5 (`Install-VcpkgPackages`) |
| loopMIDI via winget, non-fatal | Task 6 |
| Prereqs: all items checked before reporting failures | Task 7 |
| `-Skip` respected | Task 7 |
| NDI: check, browser, wait, re-validate, exit on failure | Task 8 |
| Build: cmake configure + build with config selection | Task 9 |
| Build: abort + last-30-lines on cmake failure | Task 9 |
| Build: env vars refreshed from user scope | Task 9 |
| Qt bin/ on PATH for windeployqt post-build step | Task 9 |
| Summary table with ✅/❌/⏭️ | Task 10 |
| Re-run safety (~5 s on configured machine) | All tasks (idempotent checks) |
| No self-elevation | Not needed — winget handles UAC naturally |

All spec requirements covered. ✅
