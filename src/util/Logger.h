#pragma once
#include <QString>

namespace StudioLog {

/// Simple thread-safe logger.  Writes timestamped lines to stderr and an
/// optional rolling log file.  All methods are safe to call from any thread.
class Logger
{
public:
    enum class Level { Debug, Info, Warn, Error };

    static void init(const QString& logFilePath = {});
    static void setLevel(Level minLevel);

    static void debug(const QString& msg);
    static void info (const QString& msg);
    static void warn (const QString& msg);
    static void error(const QString& msg);

    static void log(Level level, const QString& msg);

private:
    Logger() = delete;
};

} // namespace StudioLog
