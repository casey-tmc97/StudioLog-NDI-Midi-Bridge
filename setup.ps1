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
        18      { "Visual Studio 18 2026" }
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
    $vcpkgExe    = Join-Path $VcpkgPath "vcpkg.exe"
    $manifestFile = Join-Path $PSScriptRoot "vcpkg.json"

    if (Test-Path $manifestFile) {
        # Manifest mode — vcpkg.json declares rtmidi; cmake installs it automatically.
        # Pre-install here to surface errors early and speed up the first cmake configure.
        Write-Info "Installing vcpkg packages from vcpkg.json (manifest mode)…"
        Push-Location $PSScriptRoot
        try {
            & $vcpkgExe install --triplet x64-windows 2>&1 | Write-Host
            $ec = $LASTEXITCODE
        } finally {
            Pop-Location
        }
        if ($ec -ne 0) {
            Add-Failure "vcpkg install (manifest mode) failed — see output above."
        } else {
            Write-Ok "vcpkg packages installed (manifest mode)"
        }
    } else {
        # Classic mode — install rtmidi directly
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

# ─── Stage placeholders (replaced in later tasks) ─────────────────────────────
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
        Add-Failure "Build: cmake not on PATH — run '.\setup.ps1 -Stage prereqs' first."
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
        Add-Failure "Build: VCPKG_ROOT not set — run '.\setup.ps1 -Stage prereqs' first."
        return
    }
    if (-not $env:Qt6_DIR) {
        Write-Fail "Qt6_DIR not set — run '.\setup.ps1 -Stage prereqs' first."
        $script:Summary['Build'] = "❌  Qt6_DIR missing"
        Add-Failure "Build: Qt6_DIR not set — run '.\setup.ps1 -Stage prereqs' first."
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
        Add-Failure "Build: cmake configure failed — see output above."
        Show-Summary
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
        Add-Failure "Build: cmake build failed — see output above."
        Show-Summary
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
function Show-Summary {
    $inner = 58
    Write-Host ""
    Write-Host "╔$('═' * $inner)╗" -ForegroundColor Cyan
    Write-Host "║       StudioLog NDI/MIDI Bridge — Setup Summary          ║" -ForegroundColor Cyan
    Write-Host "╠$('═' * $inner)╣" -ForegroundColor Cyan

    foreach ($key in $script:Summary.Keys) {
        $val   = $script:Summary[$key]
        $color = if ($val -match '^✅')    { 'Green'  }
                 elseif ($val -match '^❌') { 'Red'    }
                 else                       { 'Yellow' }

        # ✅ and ❌ are each 1 code unit but 2 terminal columns (wide emoji).
        # ⏭️ is 2 code units and 2 terminal columns — balanced, no correction needed.
        # Reduce padTarget by the number of wide-emoji chars so the right border aligns.
        $wide      = ([regex]::Matches($val, '[✅❌]')).Count
        $padTarget = $inner - $wide
        $label     = "  {0,-16} " -f ($key + ':')              # always 19 ASCII chars

        # Max code units for $val before it overflows the inner box width:
        #   display_width($cell) = label.Length + dispVal.Length + wide ≤ padTarget
        #   → dispVal.Length ≤ padTarget - label.Length - wide
        $maxValLen = $padTarget - $label.Length - $wide
        $dispVal   = if ($val.Length -gt $maxValLen) {
                         $val.Substring(0, [Math]::Max(0, $maxValLen - 1)) + '…'
                     } else { $val }

        Write-Host ("║" + ($label + $dispVal).PadRight($padTarget) + "║") -ForegroundColor $color
    }

    Write-Host "╚$('═' * $inner)╝" -ForegroundColor Cyan
    Write-Host ""

    if ($script:Failures.Count -gt 0) {
        Write-Host "  ❌  $($script:Failures.Count) item(s) need attention — fix them, then re-run .\setup.ps1" `
            -ForegroundColor Red
    } else {
        Write-Host "  🎉  All checks passed." -ForegroundColor Green
    }
    Write-Host ""
}

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
