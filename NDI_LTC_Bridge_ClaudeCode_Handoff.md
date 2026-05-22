# StudioLog NDI/MIDI Bridge — Claude Code Handoff

Paste this document at the start of a Claude Code session to resume with full context.

---

## Project Identity

- **Name:** StudioLog NDI/MIDI Bridge
- **Organization:** Texas Music Cafe (TMC) — 501(c)(3) nonprofit, live broadcast concert production
- **Purpose:** Lightweight desktop utility that receives an NDI audio stream containing embedded SMPTE LTC timecode, decodes the LTC, and outputs MIDI Timecode (MTC) in real time
- **Target users:** Broadcast engineers / technical directors in live production environments
- **Platforms:** Windows (primary), macOS (secondary)

---

## Finalized Tech Stack

| Layer | Choice |
|-------|--------|
| Language | C++20 |
| UI | Qt 6.x Widgets only (no QML) |
| NDI Ingest | Vizrt NDI Advanced SDK (audio-only bandwidth mode) |
| LTC Decode | libltc 1.x |
| MIDI I/O | RtMidi 6.x |
| Build | CMake 3.25+ |
| Package mgr | vcpkg |
| Settings | QSettings |
| Packaging | CPack + NSIS (Windows) / .app bundle (macOS) |

**Rejected:** JUCE (unnecessary audio engine overhead), Rust (no viable NDI bindings), Electron/web frameworks (unsuitable for 24/7 broadcast), teVirtualMIDI SDK (requires distribution license)

---

## Key Architecture Decisions (Locked)

### NDI
- Use `NDIlib_recv_bandwidth_audio_only` — eliminates all video decode cost
- Discovery via `NDIlib_find_get_current_sources()` polled every 3s on a QTimer (main thread)
- Audio arrives as planar float32, typically 48kHz
- Channel selector: Left / Right / Auto-detect (peak level comparison)
- **NDI source:** A standalone Raspberry Pi appliance running an LTC generator, outputting audio over NDI. Not a TriCaster. Channel is TBD — confirm with Pi config before hardcoding default.

### LTC Decode
- libltc wrapping: `ltc_decoder_create()` → `ltc_decoder_write()` → `ltc_decoder_read()`
- Feed 256-sample chunks from ring buffer
- Auto frame-rate detection: measure inter-frame sample distance over 10 frames
- Support: 23.976, 24, 25, 29.97DF, 29.97NDF, 30
- FrameValidator: 2-frame confirmation window on lock acquisition, continuity check, dropout detection

### MTC Output
- **Virtual MIDI on Windows:** Use loopMIDI (Tobias Erichsen) as a prerequisite
  - App enumerates MIDI ports via RtMidi at startup; user selects from dropdown
  - Installer checks for loopMIDI; if absent, shows one-time download prompt
  - Do NOT hardcode a port name — user picks whatever loopMIDI port they've created
  - Settings persists selected port name; reconnects on relaunch
- **Virtual MIDI on macOS:** CoreMIDI virtual port via RtMidi `createVirtualPort()` — no driver needed
- On LTC lock: send Full Frame SysEx (`F0 7F 7F 01 01 hh mm ss ff F7`) before starting QF stream
- Quarter-frame scheduler: dedicated TIME_CRITICAL thread with sleep-to-target + busy-wait tail (not plain sleep_for)
- `timeBeginPeriod(1)` called once at Windows process startup, never released
- Freewheel: continue MTC output for up to 2 seconds on LTC dropout, then stop

### Threading Model
| Thread | Priority | Responsibility |
|--------|----------|----------------|
| Main/UI | Normal | Qt event loop, QTimer discovery, settings, state signals |
| NDI Receive | TIME_CRITICAL | `NDIlib_recv_capture_v3()`, channel extract, ring buffer write |
| LTC Decode | HIGH | Ring buffer read, libltc feed, frame validation |
| MTC Output | TIME_CRITICAL | QF sleep-to-target loop, MIDI send |

- Cross-thread UI updates: `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` only
- NDI→LTC: lock-free SPSC ring buffer (8192 samples)
- LTC→MTC: `std::atomic<LTCFrame>` + condvar wake

### State Machine
```
IDLE → CONNECTING → SEARCHING_LTC → LOCKED
LOCKED → FREEWHEEL → LOCKED (reacquired) or SEARCHING_LTC (timeout 2s)
LOCKED / CONNECTING → RECONNECTING → SEARCHING_LTC
```

---

## Repository Structure to Scaffold

```
studiolog-ndi-midi-bridge/
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json
├── README.md
├── .gitignore
├── src/
│   ├── main.cpp
│   ├── app/
│   │   ├── Application.h/.cpp
│   │   ├── AppState.h/.cpp
│   │   └── Settings.h/.cpp
│   ├── ndi/
│   │   ├── NDIDiscovery.h/.cpp
│   │   ├── NDIReceiver.h/.cpp
│   │   └── AudioRingBuffer.h
│   ├── ltc/
│   │   ├── LTCDecoder.h/.cpp
│   │   ├── FrameValidator.h/.cpp
│   │   └── FrameRateDetector.h/.cpp
│   ├── midi/
│   │   ├── MIDIOutput.h/.cpp
│   │   ├── MTCGenerator.h/.cpp
│   │   └── MTCTypes.h
│   ├── ui/
│   │   ├── MainWindow.h/.cpp
│   │   ├── MainWindow.ui
│   │   ├── TrayIcon.h/.cpp
│   │   └── SetupPanel.h/.cpp
│   └── util/
│       ├── HighResTimer.h/.cpp
│       ├── Logger.h/.cpp
│       └── PlatformInit.h/.cpp
├── third_party/
│   ├── NDI/                  ← Vizrt NDI Advanced SDK (not committed, gitignored)
│   └── libltc/               ← git submodule
├── resources/
│   ├── icons/
│   └── studiolog-ndi-midi-bridge.qrc
├── packaging/
│   ├── windows/installer.nsi
│   └── macos/
└── tests/
    ├── test_ltc_decoder.cpp
    ├── test_frame_validator.cpp
    ├── test_mtc_timing.cpp
    └── CMakeLists.txt
```

---

## First Task for Claude Code

Generate the complete repository scaffold for **StudioLog NDI/MIDI Bridge**:

1. `CMakeLists.txt` (root) — Qt6, RtMidi, libltc, NDI SDK linkage; project name `StudioLogNDIMIDIBridge`
2. `CMakePresets.json` — Debug / Release / RelWithDebInfo for Windows and macOS
3. `vcpkg.json` — RtMidi dependency
4. All `.h` and `.cpp` files as stubs with:
   - Correct `#pragma once` / include guards
   - Class declaration matching the architecture above
   - Constructor/destructor stubs
   - Key method signatures with `// TODO` bodies
   - Thread start/stop lifecycle stubs
5. `AudioRingBuffer.h` — fully implemented lock-free SPSC ring buffer (this is small and self-contained, implement it completely)
6. `MTCTypes.h` — fully implemented: `SMPTETimecode` struct, `FPS` enum, `fpsCode()`, `qfIntervalNs()`, `advanceByTwo()`
7. `.gitignore` — C++, CMake, Qt, NDI SDK, vcpkg, platform build artifacts
8. `README.md` — build instructions for Windows (MSVC + vcpkg) and macOS (Clang + Homebrew Qt6)

Do not implement full logic yet — stubs with correct signatures are the goal. `AudioRingBuffer.h` and `MTCTypes.h` are exceptions: implement those fully.

---

## Reference: MTC Quarter Frame Data Layout

```
Piece 0: frame LSN       (frame & 0x0F)
Piece 1: frame MSN       ((frame >> 4) & 0x01)
Piece 2: seconds LSN     (sec & 0x0F)
Piece 3: seconds MSN     ((sec >> 4) & 0x03)
Piece 4: minutes LSN     (mins & 0x0F)
Piece 5: minutes MSN     ((mins >> 4) & 0x03)
Piece 6: hours LSN       (hours & 0x0F)
Piece 7: hours MSN + fps ((hours >> 4) & 0x01) | (fps_code << 1)

fps_code: 0=24, 1=25, 2=29.97df, 3=30
Message format: 0xF1, (piece << 4) | data
QF sequence covers 2 frames; advance TC by 2 after piece 7
```

---

## Notes

- Full architecture document generated in prior conversation (search "NDI LTC Bridge Architecture" in past chats)
- Ask Casey which channel the Pi LTC appliance outputs on before finalizing the channel default
- Windows loopMIDI installer check: look for registry key or service name from loopMIDI install
- macOS launch agent plist template needed in `packaging/macos/`
- Internal bundle ID / reverse-DNS: `org.texasmusiccafe.studiolog-ndi-midi-bridge`
