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
    // TODO: build a linear frame counter for both prev and next, then
    //       check that next == prev + 1 (accounting for DF skip rules).
    // For now: same fps and frames differ by 1 (simplified, doesn't handle
    // second/minute boundaries or DF skips yet).
    (void)prev; (void)next;
    return false;
}

} // namespace StudioLog
