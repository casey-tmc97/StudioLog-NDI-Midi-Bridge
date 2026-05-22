# StudioLog NDI/MIDI Bridge

A lightweight broadcast utility for **Texas Music Cafe** that receives an NDI audio stream containing embedded SMPTE LTC timecode, decodes the LTC, and outputs MIDI Timecode (MTC) in real time.

---

## Requirements

| Requirement | Version |
|---|---|
| CMake | 3.25+ |
| vcpkg | latest |
| Qt | 6.x |
| Compiler (Windows) | MSVC 2022 (Visual Studio 17) |
| Compiler (macOS) | Apple Clang 15+ / Homebrew LLVM 17+ |
| NDI Advanced SDK | Latest from Vizrt |
| loopMIDI (Windows) | [tobias-erichsen.de](https://www.tobias-erichsen.de/software/loopmidi.html) |

---

## Getting the NDI SDK

The Vizrt NDI Advanced SDK is **not bundled** in this repo (it requires a free registration).

1. Go to [https://www.ndi.tv/sdk/](https://www.ndi.tv/sdk/) and download the **NDI Advanced SDK**.
2. Install/unzip it, then copy the SDK folder so the layout is:

```
third_party/NDI/
├── include/
│   └── Processing.NDI.Lib.h
├── lib/
│   ├── x64/                ← Windows import lib
│   └── macOS/              ← macOS dylib
└── bin/
    └── x64/                ← Windows runtime DLL
```

---

## Building on Windows (MSVC + vcpkg)

### 1 — Prerequisites

```powershell
# Install Visual Studio 2022 with "Desktop development with C++" workload
# Install Qt 6 via the Qt Installer (https://www.qt.io/download)
# Install vcpkg and set VCPKG_ROOT
git clone https://github.com/microsoft/vcpkg $env:VCPKG_ROOT
& "$env:VCPKG_ROOT\bootstrap-vcpkg.bat"

# Install loopMIDI from https://www.tobias-erichsen.de/software/loopmidi.html
# Create a virtual MIDI port named anything you like — you'll select it in the app.
```

### 2 — Clone with submodules

```powershell
git clone --recurse-submodules https://github.com/texas-music-cafe/studiolog-ndi-midi-bridge.git
cd studiolog-ndi-midi-bridge
```

### 3 — Configure & build (Debug)

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-debug
```

### 4 — Configure & build (Release)

```powershell
cmake --preset windows-msvc-release
cmake --build --preset windows-release
```

### 5 — Run

```powershell
.\build\windows-release\Release\StudioLogNDIMIDIBridge.exe
```

---

## Building on macOS (Apple Clang + Homebrew Qt 6)

### 1 — Prerequisites

```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install Homebrew (https://brew.sh) then:
brew install cmake ninja qt@6 pkg-config

# Add Qt to PATH (Apple Silicon example)
echo 'export PATH="/opt/homebrew/opt/qt@6/bin:$PATH"' >> ~/.zshrc
source ~/.zshrc

# Install vcpkg and set VCPKG_ROOT
git clone https://github.com/microsoft/vcpkg ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT=~/vcpkg
```

### 2 — Clone with submodules

```bash
git clone --recurse-submodules https://github.com/texas-music-cafe/studiolog-ndi-midi-bridge.git
cd studiolog-ndi-midi-bridge
```

### 3 — Configure & build (Debug)

```bash
cmake --preset macos-clang-debug
cmake --build --preset macos-debug
```

### 4 — Configure & build (Release)

```bash
cmake --preset macos-clang-release
cmake --build --preset macos-release
```

### 5 — Run

```bash
open build/macos-release/StudioLogNDIMIDIBridge.app
```

---

## Running Tests

```bash
# After a debug configure+build:
ctest --test-dir build/windows-debug --output-on-failure   # Windows
ctest --test-dir build/macos-debug   --output-on-failure   # macOS
```

---

## Architecture Overview

```
NDI Audio Stream (Pi LTC appliance)
        │ float32 planar audio @ 48kHz
        ▼
  NDIReceiver (TIME_CRITICAL thread)
        │ SPSC ring buffer (8192 samples)
        ▼
   LTCDecoder (HIGH thread)  ←── FrameRateDetector
        │ atomic<LTCFrame> + condvar
        ▼
  MTCGenerator (TIME_CRITICAL thread)
        │ sleep-to-target + busy-wait tail
        ▼
  RtMidi → loopMIDI (Windows) / CoreMIDI virtual port (macOS)
        │
        ▼
  DAW / downstream MIDI device
```

### State Machine
```
IDLE → CONNECTING → SEARCHING_LTC → LOCKED ↔ FREEWHEEL (2 s dropout window)
LOCKED / CONNECTING → RECONNECTING → SEARCHING_LTC
```

---

## Bundle ID

`org.texasmusiccafe.studiolog-ndi-midi-bridge`

---

## License

Texas Music Cafe — internal production tool. See `LICENSE` for details.
