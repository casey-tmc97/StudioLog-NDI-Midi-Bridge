#include "FrameValidator.h"

namespace StudioLog {

FrameValidator::FrameValidator() = default;
FrameValidator::~FrameValidator() = default;

bool FrameValidator::validate(const SMPTETimecode& tc)
{
    ++framesSinceValid_;

    if (!prev_.has_value()) {
        prev_ = tc;
        consecutiveCount_ = 1;
        return false;
    }

    if (!isConsecutive(*prev_, tc)) {
        // Non-consecutive frame — restart confirmation
        consecutiveCount_ = 1;
        locked_ = false;
        prev_ = tc;
        return false;
    }

    ++consecutiveCount_;
    framesSinceValid_ = 0;
    prev_ = tc;

    if (!locked_ && consecutiveCount_ >= confirmationWindow_) {
        locked_ = true;
    }

    return locked_;
}

bool FrameValidator::isDropout() const
{
    return framesSinceValid_ >= dropoutThreshold_;
}

void FrameValidator::reset()
{
    locked_           = false;
    consecutiveCount_ = 0;
    framesSinceValid_ = 0;
    prev_.reset();
}

bool FrameValidator::isConsecutive(const SMPTETimecode& prev,
                                    const SMPTETimecode& next) const
{
    // FPS mismatch → not consecutive (rate change mid-stream)
    if (prev.fps != next.fps) return false;

    const int fps = framesPerSecond(prev.fps);

    // Convert a timecode to an absolute linear frame number.
    //
    // Non-drop-frame:
    //   n = fps * (3600h + 60m + s) + f
    //
    // Drop-frame (29.97DF):
    //   Frames 0 and 1 are omitted at the start of every minute that is not
    //   a multiple of 10, so the total number of dropped frames through minute M
    //   (across all hours) is:  2 * M  -  2 * (M / 10)
    //   where M = 60*h + m (integer division for the /10 term).
    //
    //   n_df = fps * (3600h + 60m + s) + f - 2*M + 2*(M/10)
    //
    auto linearFrame = [fps](const SMPTETimecode& tc) -> int64_t {
        const int64_t totalMins =
            static_cast<int64_t>(tc.hours) * 60 + tc.minutes;
        int64_t n =
            static_cast<int64_t>(fps) *
                (static_cast<int64_t>(tc.hours)   * 3600 +
                 static_cast<int64_t>(tc.minutes) * 60   +
                 static_cast<int64_t>(tc.seconds))
            + static_cast<int64_t>(tc.frames);

        if (tc.dropFrame) {
            n -= 2 * totalMins;
            n += 2 * (totalMins / 10);
        }
        return n;
    };

    return linearFrame(next) == linearFrame(prev) + 1;
}

} // namespace StudioLog
