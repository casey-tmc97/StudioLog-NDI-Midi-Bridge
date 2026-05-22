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
    // Convert ticks → nanoseconds without overflow for typical frequencies
    return (count.QuadPart * 1'000'000'000LL) / g_qpcFreq.QuadPart;
}

void HighResTimer::sleepUntilNs(int64_t targetNs) noexcept
{
    // TODO: implement precise timed sleep using a waitable timer
    //   HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr,
    //       CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    //   LARGE_INTEGER dueTime = { .QuadPart = -(targetNs - nowNs()) / 100 };
    //   SetWaitableTimerEx(timer, &dueTime, 0, nullptr, nullptr, nullptr, 0);
    //   WaitForSingleObject(timer, INFINITE);
    //   CloseHandle(timer);
    (void)targetNs;
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
