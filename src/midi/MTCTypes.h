#pragma once
#include <cstdint>
#include <QMetaType>

namespace StudioLog {

// ─────────────────────────────────────────────────────────────────────────────
// FPS — supported SMPTE frame rates
// ─────────────────────────────────────────────────────────────────────────────
enum class FPS : uint8_t {
    FPS_23976  = 0,  ///< 23.976 fps (film pull-down)
    FPS_24     = 1,  ///< 24 fps
    FPS_25     = 2,  ///< 25 fps (EBU / PAL)
    FPS_2997DF = 3,  ///< 29.97 fps drop-frame (NTSC broadcast)
    FPS_2997NDF = 4, ///< 29.97 fps non-drop-frame
    FPS_30     = 5,  ///< 30 fps
};

/// Returns the MTC fps_code nibble embedded in QF piece 7 and Full Frame SysEx.
/// MIDI spec: 0 = 24, 1 = 25, 2 = 29.97 (DF or NDF — only one 29.97 code exists),
///            3 = 30 NDF.  Both 29.97DF and 29.97NDF use code 2; the drop-frame
///            distinction is implicit in whether frames 00/01 are skipped.
inline uint8_t fpsCode(FPS fps) noexcept {
    switch (fps) {
        case FPS::FPS_23976:   return 0;
        case FPS::FPS_24:      return 0;
        case FPS::FPS_25:      return 1;
        case FPS::FPS_2997DF:  return 2;
        case FPS::FPS_2997NDF: return 2; // 29.97 fps — no separate NDF code in MTC spec
        case FPS::FPS_30:      return 3;
        default:               return 0;
    }
}

/// Nominal integer frames-per-second (used for frame counter wrapping).
inline int framesPerSecond(FPS fps) noexcept {
    switch (fps) {
        case FPS::FPS_23976:   return 24;
        case FPS::FPS_24:      return 24;
        case FPS::FPS_25:      return 25;
        case FPS::FPS_2997DF:  return 30;
        case FPS::FPS_2997NDF: return 30;
        case FPS::FPS_30:      return 30;
        default:               return 30;
    }
}

/// Quarter-frame interval in nanoseconds.
/// One QF message must be sent per (1 frame / 4), because 8 QF messages
/// cover exactly 2 frames.
///   interval = 1e9 / fps / 4
inline int64_t qfIntervalNs(FPS fps) noexcept {
    switch (fps) {
        case FPS::FPS_23976:   return 10'427'083LL; // 1e9 / 23.976 / 4
        case FPS::FPS_24:      return 10'416'667LL; // 1e9 / 24    / 4
        case FPS::FPS_25:      return 10'000'000LL; // 1e9 / 25    / 4
        case FPS::FPS_2997DF:  return  8'341'583LL; // 1e9 / 29.97 / 4
        case FPS::FPS_2997NDF: return  8'341'583LL;
        case FPS::FPS_30:      return  8'333'333LL; // 1e9 / 30    / 4
        default:               return  8'333'333LL;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SMPTETimecode
// ─────────────────────────────────────────────────────────────────────────────
struct SMPTETimecode {
    uint8_t hours   = 0;  ///< 0–23
    uint8_t minutes = 0;  ///< 0–59
    uint8_t seconds = 0;  ///< 0–59
    uint8_t frames  = 0;  ///< 0–(framesPerSecond-1); DF skip handled in advanceByTwo()
    FPS     fps     = FPS::FPS_30;
    bool    dropFrame = false;

    bool operator==(const SMPTETimecode& o) const noexcept {
        return hours   == o.hours   &&
               minutes == o.minutes &&
               seconds == o.seconds &&
               frames  == o.frames  &&
               fps     == o.fps;
    }
    bool operator!=(const SMPTETimecode& o) const noexcept { return !(*this == o); }

    /// Advance the timecode by exactly two frames (one complete QF cycle).
    /// Handles drop-frame skip rules for 29.97DF:
    ///   Skip frames 0 & 1 at the start of every minute except multiples of 10.
    void advanceByTwo() noexcept {
        const int maxFrames = framesPerSecond(fps);
        frames = static_cast<uint8_t>(frames + 2);

        if (frames >= static_cast<uint8_t>(maxFrames)) {
            frames = static_cast<uint8_t>(frames - maxFrames);
            if (++seconds >= 60) {
                seconds = 0;
                if (++minutes >= 60) {
                    minutes = 0;
                    if (++hours >= 24) hours = 0;
                }
            }
            // 29.97DF: skip frames 0 and 1 unless this is a minute × 10
            if (dropFrame && seconds == 0 && (minutes % 10) != 0) {
                frames += 2; // guaranteed safe: maxFrames=30, frames was 0 or 1
            }
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// MTCFullFrame  — SysEx Full Frame message (10 bytes)
//   F0 7F 7F 01 01 hh mm ss ff F7
//   hh = (fps_code << 5) | hours
// ─────────────────────────────────────────────────────────────────────────────
struct MTCFullFrame {
    uint8_t data[10]{0xF0, 0x7F, 0x7F, 0x01, 0x01, 0, 0, 0, 0, 0xF7};

    explicit MTCFullFrame(const SMPTETimecode& tc) noexcept {
        data[5] = static_cast<uint8_t>((fpsCode(tc.fps) << 5) | (tc.hours & 0x1F));
        data[6] = tc.minutes;
        data[7] = tc.seconds;
        data[8] = tc.frames;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// MTCQuarterFrame — one QF MIDI message (2 bytes)
//   Status : 0xF1
//   Data   : (piece << 4) | nibble
//
// Piece mapping (from MIDI spec):
//   0  frame  LSN   (frame  & 0x0F)
//   1  frame  MSN   ((frame >> 4) & 0x01)
//   2  sec    LSN   (sec    & 0x0F)
//   3  sec    MSN   ((sec   >> 4) & 0x03)
//   4  min    LSN   (min    & 0x0F)
//   5  min    MSN   ((min   >> 4) & 0x03)
//   6  hours  LSN   (hours  & 0x0F)
//   7  hours  MSN + fps_code  ((hours >> 4) & 0x01) | (fps_code << 1)
//
// After piece 7 the timecode must be advanced by 2 frames.
// ─────────────────────────────────────────────────────────────────────────────
struct MTCQuarterFrame {
    uint8_t status = 0xF1;
    uint8_t data   = 0x00;

    /// Build one QF message for the given piece index (0–7) and timecode.
    static MTCQuarterFrame make(uint8_t piece, const SMPTETimecode& tc) noexcept {
        uint8_t nibble = 0;
        switch (piece & 0x07u) {
            case 0: nibble =  tc.frames         & 0x0Fu; break;
            case 1: nibble = (tc.frames  >> 4u) & 0x01u; break;
            case 2: nibble =  tc.seconds        & 0x0Fu; break;
            case 3: nibble = (tc.seconds >> 4u) & 0x03u; break;
            case 4: nibble =  tc.minutes        & 0x0Fu; break;
            case 5: nibble = (tc.minutes >> 4u) & 0x03u; break;
            case 6: nibble =  tc.hours          & 0x0Fu; break;
            case 7: nibble = static_cast<uint8_t>(
                                 ((tc.hours >> 4u) & 0x01u) |
                                 (fpsCode(tc.fps) << 1u));
                    break;
            default: break;
        }
        MTCQuarterFrame qf;
        qf.data = static_cast<uint8_t>((piece << 4u) | nibble);
        return qf;
    }
};

} // namespace StudioLog

// Required for Qt QueuedConnection to serialise SMPTETimecode across the event
// loop (e.g. frameDecoded → setTimecode, timecodeUpdated → onTimecodeUpdated).
// Must appear outside any namespace.
Q_DECLARE_METATYPE(StudioLog::SMPTETimecode)
