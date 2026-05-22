#include "NDIDiscovery.h"
#include "util/Logger.h"
#if __has_include(<Processing.NDI.Lib.h>)
#  include <Processing.NDI.Lib.h>
#  define HAS_NDI_SDK 1
#endif

namespace StudioLog {

NDIDiscovery::NDIDiscovery(QObject* parent)
    : QObject(parent)
{
    pollTimer_.setInterval(3000);
    connect(&pollTimer_, &QTimer::timeout, this, &NDIDiscovery::poll);
}

NDIDiscovery::~NDIDiscovery()
{
    stop();
}

void NDIDiscovery::start()
{
    if (ndiInitialized_) return;
    if (!initNDI()) return;
    pollTimer_.start();
    poll(); // immediate first scan
}

void NDIDiscovery::stop()
{
    pollTimer_.stop();
    shutdownNDI();
}

QStringList NDIDiscovery::sources() const
{
    return currentSources_;
}

bool NDIDiscovery::initNDI()
{
    // TODO: call NDIlib_initialize() (once, globally — coordinate with NDIReceiver)
    // TODO: create NDIlib_find_instance_t with NDIlib_find_create_v2()
    // TODO: store handle in findInstance_
    // TODO: set ndiInitialized_ = true on success
    Logger::info("NDIDiscovery: NDI not yet initialized (stub)");
    return false;
}

void NDIDiscovery::shutdownNDI()
{
    if (!ndiInitialized_) return;
    // TODO: NDIlib_find_destroy(findInstance_)
    findInstance_   = nullptr;
    ndiInitialized_ = false;
}

void NDIDiscovery::poll()
{
    if (!ndiInitialized_) return;

    // TODO:
    //   uint32_t count = 0;
    //   const NDIlib_source_t* srcs =
    //       NDIlib_find_get_current_sources(findInstance_, &count);
    //   Build QStringList from srcs[i].p_ndi_name
    //   If list != currentSources_, update and emit sourcesChanged()
}

} // namespace StudioLog
