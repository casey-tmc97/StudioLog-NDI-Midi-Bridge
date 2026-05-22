#include "FrameRateDetector.h"
#include <cmath>
#include <numeric>

namespace StudioLog {

FrameRateDetector::FrameRateDetector() = default;
FrameRateDetector::~FrameRateDetector() = default;

FPS FrameRateDetector::feed(long sampleEndPos, int sampleRate)
{
    endPositions_[count_ % WINDOW] = sampleEndPos;
    ++count_;

    if (count_ < WINDOW + 1) return detected_; // not enough data yet

    // Compute mean inter-frame distance over the window
    long totalDelta = 0;
    for (int i = 1; i < WINDOW; ++i) {
        int a = (count_ - WINDOW + i - 1) % WINDOW;
        int b = (count_ - WINDOW + i)     % WINDOW;
        totalDelta += endPositions_[b] - endPositions_[a];
    }
    double meanSpf    = static_cast<double>(totalDelta) / (WINDOW - 1);
    double measuredFps = static_cast<double>(sampleRate) / meanSpf;

    spf_     = static_cast<int>(std::round(meanSpf));
    detected_ = mapToFPS(measuredFps);
    return detected_;
}

FPS FrameRateDetector::mapToFPS(double fps) const
{
    // Tolerances: ±0.5 fps for integer rates, tight window for 23.976 / 29.97
    if (fps > 23.5  && fps < 24.2) {
        return (fps < 23.99) ? FPS::FPS_23976 : FPS::FPS_24;
    }
    if (fps > 24.5  && fps < 25.5) return FPS::FPS_25;
    if (fps > 29.5  && fps < 30.5) {
        return (fps < 29.99) ? FPS::FPS_2997DF : FPS::FPS_30;
        // Note: cannot distinguish 29.97DF from 29.97NDF from sample timing alone.
        // FrameValidator or LTC bit stream parsing is needed for that distinction.
    }
    return detected_; // keep previous if measurement is ambiguous
}

void FrameRateDetector::reset()
{
    count_    = 0;
    detected_ = FPS::FPS_30;
    spf_      = 1601;
    endPositions_.fill(0);
}

} // namespace StudioLog
