#include "PlatformInit.h"
#include "HighResTimer.h"

#ifdef _WIN32
#  include <windows.h>
#  include <mmsystem.h>
#  pragma comment(lib, "winmm.lib")
#endif

namespace StudioLog {

void PlatformInit::init() noexcept
{
#ifdef _WIN32
    timeBeginPeriod(1); // Set timer resolution to 1 ms globally for this process
#endif
    HighResTimer::init();
}

void PlatformInit::shutdown() noexcept
{
#ifdef _WIN32
    timeEndPeriod(1);
#endif
}

} // namespace StudioLog
