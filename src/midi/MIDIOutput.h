#pragma once
#include <QObject>
#include <QStringList>
#include <memory>
#include <cstdint>

// Forward declaration — avoid leaking RtMidi.h into all TUs
// vcpkg installs to rtmidi/RtMidi.h
class RtMidiOut;
class QTimer;

namespace StudioLog {

/// Thin wrapper around RtMidi for MIDI output port management.
///
/// Enumerates available ports at startup and on request.  Opens the port whose
/// name matches the saved setting; exposes sendRaw() for the MTC generator.
///
/// Windows: user must have loopMIDI installed and at least one virtual port
///   created.  The installer checks for loopMIDI; if absent, shows a prompt.
/// macOS: RtMidi creates a CoreMIDI virtual port — no driver needed.
class MIDIOutput : public QObject
{
    Q_OBJECT

public:
    explicit MIDIOutput(QObject* parent = nullptr);
    ~MIDIOutput() override;

    /// Enumerate available MIDI output port names.
    QStringList availablePorts() const;

    /// Open the port whose name contains @p portName (case-insensitive partial match).
    /// Returns true on success.
    bool openPort(const QString& portName);

    /// Close the current port without destroying the RtMidi instance.
    void closePort();

    bool isOpen() const { return portOpen_; }
    QString currentPortName() const { return currentPortName_; }

    /// Send raw bytes.  Must be called from the MTC output thread only.
    void sendRaw(const uint8_t* data, std::size_t len);

    /// Convenience: send a 2-byte quarter-frame message (0xF1, data).
    void sendQuarterFrame(uint8_t data);

    /// Convenience: send a 10-byte Full Frame SysEx.
    void sendFullFrame(const uint8_t* sysex10);

    /// Check whether loopMIDI appears to be installed (Windows only).
    static bool isLoopMIDIInstalled();

    /// Start polling for MIDI port list changes every @p intervalMs milliseconds.
    /// Emits portsChanged() when the list changes (new port added / removed).
    void startPortPolling(int intervalMs = 3000);

signals:
    void portOpened(const QString& name);
    void portClosed();
    void portsChanged(const QStringList& ports);

private slots:
    void pollPorts();

private:
    std::unique_ptr<RtMidiOut> midi_;
    bool    portOpen_       = false;
    QString currentPortName_;
    QTimer* pollTimer_      = nullptr;
    QStringList lastKnownPorts_;
};

} // namespace StudioLog
