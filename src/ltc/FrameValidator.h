#pragma once
#include "midi/MTCTypes.h"
#include <optional>

namespace StudioLog {

/// Validates incoming decoded LTC frames before signalling lock.
///
/// Lock acquisition requires two consecutive consistent frames (confirmation
/// window).  After lock, each new frame is checked for continuity (frame
/// counter incremented by exactly 1, timestamps sequential).
///
/// Dropout detection: if no valid frame arrives within the dropout window
/// (configurable, default 2 s worth of frames), isDropout() returns true.
class FrameValidator
{
public:
    FrameValidator();
    ~FrameValidator();

    /// Feed one decoded timecode.  Returns true if the frame passes validation
    /// (i.e., the caller should forward it downstream).
    /// Must be called from the LTC decode thread only.
    bool validate(const SMPTETimecode& tc);

    /// True if we have satisfied the lock confirmation window.
    bool isLocked() const { return locked_; }

    /// True if we have not seen a valid frame for dropoutFrames_ consecutive polls.
    bool isDropout() const;

    /// Reset all state (e.g. after reconnect).
    void reset();

    /// How many consecutive matching frames are required before declaring lock.
    void setConfirmationWindow(int frames) { confirmationWindow_ = frames; }

    /// How many frames of silence trigger a dropout.
    void setDropoutThreshold(int frames) { dropoutThreshold_ = frames; }

private:
    bool isConsecutive(const SMPTETimecode& prev, const SMPTETimecode& next) const;

    bool          locked_             = false;
    int           confirmationWindow_ = 2;   ///< frames required for lock
    int           dropoutThreshold_   = 60;  ///< ~2 s at 30 fps
    int           consecutiveCount_   = 0;
    int           framesSinceValid_   = 0;
    std::optional<SMPTETimecode> prev_;
};

} // namespace StudioLog
