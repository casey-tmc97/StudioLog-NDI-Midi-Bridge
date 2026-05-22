#pragma once
#include <QObject>
#include <QString>

namespace StudioLog {

/// Persistent settings backed by QSettings.
///
/// Keys are read once at startup and written immediately when changed via setters.
/// All access must occur on the main thread.
class Settings : public QObject
{
    Q_OBJECT

public:
    explicit Settings(QObject* parent = nullptr);
    ~Settings() override;

    // ── NDI ──────────────────────────────────────────────────────────────────
    /// Last-used NDI source name (empty = none selected)
    QString ndiSourceName() const;
    void    setNdiSourceName(const QString& name);

    /// Channel to extract LTC from: 0 = Left, 1 = Right, -1 = Auto
    int  ltcChannel() const;
    void setLtcChannel(int ch);

    // ── MIDI ─────────────────────────────────────────────────────────────────
    /// Name of the RtMidi output port to open (loopMIDI on Windows)
    QString midiPortName() const;
    void    setMidiPortName(const QString& name);

    // ── UI ───────────────────────────────────────────────────────────────────
    bool startMinimized() const;
    void setStartMinimized(bool v);

    bool minimizeToTray() const;
    void setMinimizeToTray(bool v);

signals:
    void ndiSourceNameChanged(const QString& name);
    void ltcChannelChanged(int ch);
    void midiPortNameChanged(const QString& name);

private:
    class Impl;
    std::unique_ptr<Impl> d_;
};

} // namespace StudioLog
