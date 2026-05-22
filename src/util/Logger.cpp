#include "Logger.h"
#include <QDateTime>
#include <QFile>
#include <QMutex>
#include <QTextStream>
#include <iostream>

namespace StudioLog {

namespace {
    QMutex          g_mutex;
    Logger::Level   g_minLevel = Logger::Level::Info;
    QFile           g_file;
    Logger::LogCallback g_callback;
}

static const char* levelStr(Logger::Level l) {
    switch (l) {
        case Logger::Level::Debug: return "DEBUG";
        case Logger::Level::Info:  return "INFO ";
        case Logger::Level::Warn:  return "WARN ";
        case Logger::Level::Error: return "ERROR";
        default:                   return "?    ";
    }
}

void Logger::setCallback(LogCallback cb)
{
    QMutexLocker lk(&g_mutex);
    g_callback = std::move(cb);
}

void Logger::init(const QString& logFilePath)
{
    QMutexLocker lk(&g_mutex);
    if (!logFilePath.isEmpty()) {
        g_file.setFileName(logFilePath);
        g_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    }
}

void Logger::setLevel(Level minLevel)
{
    QMutexLocker lk(&g_mutex);
    g_minLevel = minLevel;
}

void Logger::log(Level level, const QString& msg)
{
    QMutexLocker lk(&g_mutex);
    if (level < g_minLevel) return;

    QString line = QString("[%1] [%2] %3")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"))
        .arg(QLatin1String(levelStr(level)))
        .arg(msg);

    std::cerr << line.toStdString() << '\n';

    if (g_file.isOpen()) {
        QTextStream ts(&g_file);
        ts << line << '\n';
    }

    if (g_callback) g_callback(level, line);
}

void Logger::debug(const QString& msg) { log(Level::Debug, msg); }
void Logger::info (const QString& msg) { log(Level::Info,  msg); }
void Logger::warn (const QString& msg) { log(Level::Warn,  msg); }
void Logger::error(const QString& msg) { log(Level::Error, msg); }

} // namespace StudioLog
