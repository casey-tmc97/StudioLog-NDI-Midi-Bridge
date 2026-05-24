#include "HighResTimer.h"

#ifdef _WIN32
#  include <windows.h>
#elif defined(__APPLE__)
#  include <mach/mach_time.h>
#else
#  include <chrono>
#endif

namespace StudioLog {

#ifdef _WIN32

static LARGE_INTEGER g_qpcFreq{};

void HighResTimer::init() noexcept
{
    QueryPerformanceFrequency(&g_qpcFreq);
}

int64_t HighResTimer::nowNs() noexcept
{
    LARGE_INTEGER count;
    QueryPerformanceCounter(&count);
    // Safe conversion: split to avoid int64 overflow when count is large.
    // Direct (count * 1e9 / freq) overflows once the system has been running
    // ~15 minutes at 10 MHz QPC, producing wrong absolute timestamps.
    const int64_t freq = g_qpcFreq.QuadPart;
    return (count.QuadPart / freq) * 1'000'000'000LL
         + (count.QuadPart % freq) * 1'000'000'000LL / freq;
}

// Thread-local high-resolution waitable timer (CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
// requires Windows 10 version 1803 / build 17134).  Falls back to sleep_for on
// older systems.  Timer handle is created once per thread and never closed (the
// process outlives the thread; the OS cleans up on process exit).
static HANDLE getHighResWaitableTimer() noexcept
{
    static thread_local HANDLE s_timer = []() -> HANDLE {
        HANDLE h = CreateWaitableTimerExW(nullptr, nullptr,
            CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        if (!h) {
            // Older Windows: fall back to low-resolution timer
            h = CreateWaitableTimerW(nullptr, /*bManualReset=*/TRUE, nullptr);
        }
        return h;
    }();
    return s_timer;
}

void HighResTimer::sleepUntilNs(int64_t targetNs) noexcept
{
    const int64_t remainingNs = targetNs - nowNs();
    if (remainingNs <= 0) return;

    HANDLE timer = getHighResWaitableTimer();
    if (!timer) {
        // Extreme fallback: busy-spin (should never reach this)
        while (nowNs() < targetNs) {}
        return;
    }

    // Negative LARGE_INTEGER = relative time in 100-ns units
    LARGE_INTEGER dueTime;
    dueTime.QuadPart = -(remainingNs / 100LL);
    if (dueTime.QuadPart == 0) dueTime.QuadPart = -1; // at least 100 ns

    if (SetWaitableTimer(timer, &dueTime, 0, nullptr, nullptr, FALSE)) {
        WaitForSingleObject(timer, INFINITE);
    }
}

#elif defined(__APPLE__)

static mach_timebase_info_data_t g_timebase{};

void HighResTimer::init() noexcept
{
    mach_timebase_info(&g_timebase);
}

int64_t HighResTimer::nowNs() noexcept
{
    uint64_t t = mach_absolute_time();
    return static_cast<int64_t>(t * g_timebase.numer / g_timebase.denom);
}

void HighResTimer::sleepUntilNs(int64_t targetNs) noexcept
{
    // TODO: mach_wait_until() for sub-millisecond accuracy
    (void)targetNs;
}

#else // Fallback

void HighResTimer::init() noexcept {}

int64_t HighResTimer::nowNs() noexcept
{
    using namespace std::chrono;
    return duration_cast<nanoseconds>(
        steady_clock::now().time_since_epoch()).count();
}

void HighResTimer::sleepUntilNs(int64_t targetNs) noexcept
{
    using namespace std::chrono;
    auto tp = steady_clock::time_point{nanoseconds{targetNs}};
    std::this_thread::sleep_until(tp);
}

#endif

} // namespace StudioLog
