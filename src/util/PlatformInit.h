#pragma once

namespace StudioLog {

/// One-time process-level platform initialisation.
///
/// Windows: calls timeBeginPeriod(1) so that sleep granularity is 1 ms.
///          This is called once in Application::initPlatform() and matched by
///          timeEndPeriod(1) in shutdown().  The period is intentionally never
///          released until process exit to avoid races with the MTC thread.
///
/// macOS:   no-op (Mach timers are already high-resolution).
class PlatformInit
{
public:
    static void init()     noexcept;
    static void shutdown() noexcept;

private:
    PlatformInit() = delete;
};

} // namespace StudioLog
