#pragma once
#include <cstdint>

namespace StudioLog {

/// High-resolution monotonic clock in nanoseconds.
///
/// Windows: QueryPerformanceCounter (requires timeBeginPeriod(1) at startup).
/// macOS:   mach_absolute_time() converted via mach_timebase_info.
/// Fallback: std::chrono::steady_clock (always safe, may be lower resolution).
class HighResTimer
{
public:
    /// Current time in nanoseconds (monotonic, arbitrary epoch).
    static int64_t nowNs() noexcept;

    /// Sleep the calling thread until the given absolute nanosecond timestamp.
    /// More precise than std::this_thread::sleep_until on Windows.
    static void sleepUntilNs(int64_t targetNs) noexcept;

    /// Calibrate / cache platform-specific state (call once at startup).
    static void init() noexcept;

private:
    HighResTimer() = delete;
};

} // namespace StudioLog
