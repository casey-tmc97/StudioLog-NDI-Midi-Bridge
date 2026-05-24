#include "NDIDiscovery.h"
#include "util/Logger.h"
#include <Processing.NDI.Lib.h>
#include <mutex>

namespace StudioLog {

// ── Process-level NDI init ────────────────────────────────────────────────────
// NDIlib_initialize() / NDIlib_destroy() are process-global.  We use a
// call_once so both NDIDiscovery and NDIReceiver can safely call ensureNDIInit()
// without racing.
namespace {

std::once_flag g_ndiOnce;
bool           g_ndiOk = false;

bool ensureNDIInit()
{
    std::call_once(g_ndiOnce, []() {
        g_ndiOk = NDIlib_initialize();
        if (g_ndiOk)
            Logger::info(QString("NDI runtime: %1").arg(NDIlib_version()));
        else
            Logger::error("NDIlib_initialize() failed — CPU may not support SSE4.2");
    });
    return g_ndiOk;
}

} // anonymous namespace

// ── NDIDiscovery ─────────────────────────────────────────────────────────────

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
    poll(); // immediate first scan so the UI is populated at launch
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
    if (!ensureNDIInit()) return false;

    NDIlib_find_create_t cfg{};
    cfg.show_local_sources = true;   // include Pi on the same subnet
    cfg.p_groups           = nullptr; // all groups
    cfg.p_extra_ips        = nullptr; // rely on mDNS; add Pi IP here if mDNS fails

    findInstance_ = NDIlib_find_create_v2(&cfg);
    if (!findInstance_) {
        Logger::error("NDIDiscovery: NDIlib_find_create_v2() returned NULL");
        return false;
    }

    ndiInitialized_ = true;
    Logger::info("NDIDiscovery: finder created, polling every 3 s");
    return true;
}

void NDIDiscovery::shutdownNDI()
{
    if (!ndiInitialized_) return;

    NDIlib_find_destroy(static_cast<NDIlib_find_instance_t>(findInstance_));
    findInstance_   = nullptr;
    ndiInitialized_ = false;
    Logger::info("NDIDiscovery: finder destroyed");

    // Match the NDIlib_initialize() called in ensureNDIInit().  This shuts down
    // NDI's internal threads cleanly so the process can exit without hanging.
    // Called here because shutdownNDI() is always the last NDI teardown step —
    // NDIReceiver::disconnect() (recv instance) runs before NDIDiscovery::stop().
    NDIlib_destroy();
    Logger::info("NDIDiscovery: NDI library released");
}

void NDIDiscovery::poll()
{
    if (!ndiInitialized_) return;

    uint32_t count = 0;
    const NDIlib_source_t* srcs = NDIlib_find_get_current_sources(
        static_cast<NDIlib_find_instance_t>(findInstance_), &count);

    QStringList newSources;
    newSources.reserve(static_cast<int>(count));
    for (uint32_t i = 0; i < count; ++i) {
        if (srcs[i].p_ndi_name && srcs[i].p_ndi_name[0] != '\0')
            newSources << QString::fromUtf8(srcs[i].p_ndi_name);
    }

    if (newSources != currentSources_) {
        currentSources_ = newSources;
        Logger::info(QString("NDIDiscovery: %1 source(s) — %2")
                         .arg(count)
                         .arg(newSources.join(", ")));
        emit sourcesChanged(currentSources_);
    }
}

} // namespace StudioLog
