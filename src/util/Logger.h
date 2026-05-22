#pragma once
#include <QString>
#include <functional>

namespace StudioLog {

/// Simple thread-safe logger.  Writes timestamped lines to stderr and an
/// optional rolling log file.  All methods are safe to call from any thread.
class Logger
{
public:
    enum class Level { Debug, Info, Warn, Error };

    /// Callback type: receives the fully-formatted log line (with timestamp).
    using LogCallback = std::function<void(Level, const QString& line)>;

    static void init(const QString& logFilePath = {});
    static void setLevel(Level minLevel);

    /// Install a callback invoked (under the internal mutex) after each log
    /// line is written.  Pass {} to clear.  The callback must not itself call
    /// Logger methods (deadlock).  Use QMetaObject::invokeMethod with
    /// QueuedConnection to forward to the UI thread safely.
    static void setCallback(LogCallback cb);

    static void debug(const QString& msg);
    static void info (const QString& msg);
    static void warn (const QString& msg);
    static void error(const QString& msg);

    static void log(Level level, const QString& msg);

private:
    Logger() = delete;
};

} // namespace StudioLog
