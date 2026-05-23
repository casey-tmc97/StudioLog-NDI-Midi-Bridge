# Setup Script Design
**Date:** 2026-05-23  
**Project:** StudioLog NDI/MIDI Bridge  
**Author:** casey@texasmusiccafe.org  

---

## Goal

A single `setup.ps1` at the project root that fully automates the Windows build
environment for StudioLog NDI/MIDI Bridge — from zero to a running Debug binary —
while being safely re-runnable at any stage.

---

## Script Interface

**File:** `setup.ps1` (project root)

```powershell
.\setup.ps1 [-Stage  <'all' | 'prereqs' | 'ndi' | 'build'>]   # default: all
            [-Skip   <string[]>]          # e.g. -Skip qt,loopmidi
            [-Config <'Debug' | 'Release'>]                    # default: Debug
            [-Qt6Dir <path>]              # override Qt6 auto-detect
            [-VcpkgRoot <path>]           # override vcpkg auto-detect
```

### Common invocations

| Scenario | Command |
|---|---|
| First-time setup + build | `.\setup.ps1` |
| Re-enter after placing NDI SDK | `.\setup.ps1` (prereqs skip green; NDI validates; build runs) |
| Build Release | `.\setup.ps1 -Config Release` |
| Skip loopMIDI (already installed) | `.\setup.ps1 -Skip loopmidi` |
| Re-run prereqs only | `.\setup.ps1 -Stage prereqs` |

---

## Stages

### Stage 1 — `prereqs`

Validates and installs each build prerequisite. Each check is idempotent: if
the item is already present, print ✅ and skip. If missing, install silently
via winget or perform the action described below.

| Item | Detection | Install action | Notes |
|---|---|---|---|
| **CMake** | `Get-Command cmake` or VS-bundled path | Already bundled with VS BuildTools — add to session `$PATH` | Prefer VS-bundled cmake to avoid version conflicts |
| **VS BuildTools generator** | `vswhere.exe` — read `installationVersion` major | Detect major version (`17` → VS 2022, `18` → VS 2025); auto-patch `CMakePresets.json` generator string to match | Keeps preset in sync as VS upgrades |
| **Qt 6** | `$Qt6_DIR` env var and `Qt6Config.cmake` presence | `winget install Qt.Qt.6.8.3.win64_msvc2022_64` then set `Qt6_DIR` user env var | Specific version pinned; script can be updated to newer Qt 6 minor |
| **vcpkg** | `$VCPKG_ROOT` env var and `vcpkg.exe` presence | `git clone https://github.com/microsoft/vcpkg $HOME\vcpkg` + `bootstrap-vcpkg.bat`, set `VCPKG_ROOT` user env var | Skips entirely if already configured |
| **loopMIDI** | Registry key presence or process check | `winget install TobiasErichsen.loopMIDI`; if winget unavailable, open browser and warn | Runtime requirement only; setup continues on failure |

**CMakePresets.json patching detail:**  
`vswhere` reports the numeric major. The script maps:
- major `17` → generator `"Visual Studio 17 2022"`
- major `18` → generator `"Visual Studio 18 2025"`
- unknown → warn and leave preset unchanged

The patch is applied in-place with a regex replacement on the generator string
for all three Windows `configurePresets`. The change is committed to git with
message `chore: sync CMakePresets.json generator to installed VS version`.

### Stage 2 — `ndi`

The NDI Advanced SDK requires a free registration at Vizrt and cannot be
downloaded silently.

1. Check if `third_party/NDI/include/Processing.NDI.Lib.h` exists.
2. If present → ✅ skip.
3. If missing:
   - Print the expected directory layout (from README).
   - Open `https://ndi.video/for-developers/ndi-sdk/` in the default browser.
   - Print: *"Download the NDI Advanced SDK, unzip it, and place it so the path above exists. Press Enter when ready."*
   - Wait for `Read-Host`.
   - Re-validate the layout; on failure print which paths are still missing and
     exit stage with error.

### Stage 3 — `build`

1. Augment session `$PATH` with: VS-bundled CMake, Qt6 `bin/`, vcpkg.
2. Run `cmake --preset windows-msvc-debug` (or `-release` for `Release` config).
3. Run `cmake --build --preset windows-debug` (or `-release`).
4. On success: print the full path to the built `.exe`.
5. On cmake failure: print the last 30 lines of output and exit with code 1.

---

## Output & Error Handling

### Color scheme

| Color | Meaning |
|---|---|
| 🟢 Green | Step succeeded or already satisfied |
| 🟡 Yellow | Skipped (already done or `-Skip` flag) |
| 🔴 Red | Failed — includes plain-English fix hint |
| ⚪ White | Informational / in-progress |

### Error philosophy

- `prereqs` stage: a failed item does **not** abort the stage. All items are
  checked. Failures are collected and printed in a single remediation block at
  the end of the stage, so the user sees everything to fix in one pass.
- `ndi` stage: if layout validation fails after the user confirms placement,
  the script prints which specific paths are missing and exits with code 1.
- `build` stage: aborts immediately on cmake configure or build failure and
  prints the last 30 lines of cmake output.

### Final summary table

Printed at the end of a full run:

```
╔══════════════════════════════════════════════════════╗
║       StudioLog NDI/MIDI Bridge — Setup Summary      ║
╠══════════════════════════════════════════════════════╣
║  CMake      ✅  4.2.3  (VS BuildTools)               ║
║  Qt 6       ✅  6.8.3  C:\Qt\6.8.3\msvc2022_64       ║
║  vcpkg      ✅  2024.x  C:\Users\Admin\vcpkg          ║
║  loopMIDI   ✅  installed                             ║
║  NDI SDK    ✅  third_party/NDI/include/...h          ║
║  Build      ✅  build\windows-debug\Debug\...exe      ║
╚══════════════════════════════════════════════════════╝
```

### Re-run safety

Every installer check runs `Get-Command` / `Test-Path` / env-var presence
before taking action. A fully-configured machine completes in ~5 seconds with
all green checkmarks.

### Elevation

The script does **not** self-elevate. winget does not require elevation for
per-user installs. If a package requires elevation, winget's own UAC prompt
will appear naturally.

---

## Files Changed

| Path | Change |
|---|---|
| `setup.ps1` | New file — the setup script |
| `CMakePresets.json` | Generator string patched to match installed VS version |
| `docs/superpowers/specs/2026-05-23-setup-script-design.md` | This spec |

---

## Out of Scope

- macOS setup (separate concern; README already has manual steps)
- CI/CD integration (no `-CI` flag in this version)
- vcpkg package manifest (`vcpkg.json`) — rtmidi is already found by cmake via vcpkg classic mode
