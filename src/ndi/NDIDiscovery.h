#pragma once
#include <QObject>
#include <QStringList>
#include <QTimer>

namespace StudioLog {

/// Polls NDIlib for available NDI sources on a 3-second timer (main thread).
///
/// Emits sourcesChanged() whenever the list changes.  The UI populates its
/// source dropdown from this signal.
class NDIDiscovery : public QObject
{
    Q_OBJECT

public:
    explicit NDIDiscovery(QObject* parent = nullptr);
    ~NDIDiscovery() override;

    /// Start periodic discovery.  Safe to call multiple times (idempotent).
    void start();

    /// Stop discovery and release the NDIlib find instance.
    void stop();

    /// Current list of discovered source names (display strings).
    QStringList sources() const;

signals:
    /// Emitted on the main thread when the source list changes.
    void sourcesChanged(const QStringList& sources);

private slots:
    void poll();

private:
    bool initNDI();
    void shutdownNDI();

    void*       findInstance_ = nullptr; ///< NDIlib_find_instance_t
    QStringList currentSources_;
    QTimer      pollTimer_;
    bool        ndiInitialized_ = false;
};

} // namespace StudioLog
