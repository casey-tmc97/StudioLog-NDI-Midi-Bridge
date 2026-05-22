#pragma once
#include "midi/MTCTypes.h"
#include <cstdint>
#include <array>

namespace StudioLog {

/// Measures the inter-frame sample distance over a rolling window of 10 frames
/// and maps it to the nearest supported SMPTE frame rate.
///
/// Must be called from the LTC decode thread only.
class FrameRateDetector
{
public:
    FrameRateDetector();
    ~FrameRateDetector();

    /// Feed the sample position of the end of a decoded LTC frame.
    /// Call once per frame.  Returns the detected FPS once confident (after
    /// WINDOW frames); returns FPS::FPS_30 as default until then.
    FPS feed(long sampleEndPos, int sampleRate = 48000);

    /// Last confidently detected frame rate.
    FPS detectedFPS() const { return detected_; }

    /// Samples-per-frame estimate (useful for re-initialising the libltc decoder).
    int samplesPerFrame() const { return spf_; }

    /// Reset accumulated history.
    void reset();

private:
    FPS mapToFPS(double measuredFps) const;

    static constexpr int WINDOW = 10;

    std::array<long, WINDOW> endPositions_{};
    int    count_    = 0;
    FPS    detected_ = FPS::FPS_30;
    int    spf_      = 1601; ///< 48000 / 29.97 ≈ 1601
};

} // namespace StudioLog
