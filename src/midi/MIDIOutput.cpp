#include "MIDIOutput.h"
#include "util/Logger.h"
#include <rtmidi/RtMidi.h>
#include <QMetaObject>
#include <vector>
#include <string>

#ifdef _WIN32
#  include <windows.h>
#endif

namespace StudioLog {

// ── Construction / destruction ────────────────────────────────────────────────

MIDIOutput::MIDIOutput(QObject* parent)
    : QObject(parent)
{
    try {
        midi_ = std::make_unique<RtMidiOut>(RtMidi::UNSPECIFIED, "StudioLog MTC");

        // Replace the default "throw on error" behaviour with a log-only callback
        // so transient send errors don't terminate the MTC output thread.
        // RtMidiErrorCallback = void(*)(RtMidiError::Type, const std::string&, void*)
        midi_->setErrorCallback(
            [](RtMidiError::Type /*type*/, const std::string& errorText, void* /*ctx*/) {
                Logger::warn(QString("RtMidi: %1").arg(QString::fromStdString(errorText)));
            }, nullptr);

        Logger::info(QString("MIDIOutput: RtMidiOut created (%1 port(s) available)")
                         .arg(midi_->getPortCount()));
    } catch (const RtMidiError& e) {
        Logger::error(QString("MIDIOutput: init failed — %1")
                          .arg(QString::fromStdString(e.getMessage())));
        // midi_ remains nullptr; all methods guard against this
    }
}

MIDIOutput::~MIDIOutput()
{
    closePort();
}

// ── Port enumeration ──────────────────────────────────────────────────────────

QStringList MIDIOutput::availablePorts() const
{
    QStringList names;
    if (!midi_) return names;

    try {
        const unsigned int count = midi_->getPortCount();
        names.reserve(static_cast<int>(count));
        for (unsigned i = 0; i < count; ++i)
            names << QString::fromStdString(midi_->getPortName(i));
    } catch (const RtMidiError& e) {
        Logger::warn(QString("MIDIOutput: getPortName error — %1")
                         .arg(QString::fromStdString(e.getMessage())));
    }
    return names;
}

// ── Port open / close ─────────────────────────────────────────────────────────

bool MIDIOutput::openPort(const QString& portName)
{
    closePort();
    if (!midi_) return false;

    QStringList ports = availablePorts();

    // Always emit a fresh list so the UI stays in sync
    emit portsChanged(ports);

    if (ports.isEmpty()) {
        Logger::warn("MIDIOutput: no MIDI output ports found");
        return false;
    }

    // Case-insensitive partial match — lets "loopMIDI" match "loopMIDI Port 1", etc.
    for (int i = 0; i < ports.size(); ++i) {
        if (ports[i].contains(portName, Qt::CaseInsensitive)) {
            try {
                midi_->openPort(static_cast<unsigned>(i), "StudioLog MTC Out");
            } catch (const RtMidiError& e) {
                Logger::error(QString("MIDIOutput: openPort(\"%1\") failed — %2")
                                  .arg(ports[i])
                                  .arg(QString::fromStdString(e.getMessage())));
                return false;
            }
            portOpen_        = true;
            currentPortName_ = ports[i];
            Logger::info("MIDIOutput: opened \"" + currentPortName_ + "\"");
            emit portOpened(currentPortName_);
            return true;
        }
    }

    Logger::warn(QString("MIDIOutput: no port matching \"%1\" found in: %2")
                     .arg(portName)
                     .arg(ports.join(", ")));
    return false;
}

void MIDIOutput::closePort()
{
    if (!portOpen_) return;

    if (midi_) {
        try {
            midi_->closePort();
        } catch (const RtMidiError& e) {
            Logger::warn(QString("MIDIOutput: closePort error — %1")
                             .arg(QString::fromStdString(e.getMessage())));
        }
    }

    portOpen_        = false;
    currentPortName_.clear();
    Logger::info("MIDIOutput: port closed");
    emit portClosed();
}

// ── Send (hot path — called from MTC output thread) ──────────────────────────

void MIDIOutput::sendRaw(const uint8_t* data, std::size_t len)
{
    if (!portOpen_ || !midi_) return;

    // Use the const-pointer + size overload — no std::vector allocation on the hot path.
    // RtMidiError here means the port disconnected at runtime (e.g. loopMIDI killed).
    try {
        midi_->sendMessage(data, len);
    } catch (const RtMidiError& e) {
        Logger::warn(QString("MIDIOutput: sendMessage failed — %1")
                         .arg(QString::fromStdString(e.getMessage())));

        // Mark closed immediately so the MTC thread stops sending.
        portOpen_ = false;

        // Emit portClosed() on the main thread (we are on the MTC output thread here).
        QMetaObject::invokeMethod(this, [this] {
            currentPortName_.clear();
            emit portClosed();
        }, Qt::QueuedConnection);
    }
}

void MIDIOutput::sendQuarterFrame(uint8_t data)
{
    const uint8_t msg[2] = {0xF1, data};
    sendRaw(msg, 2);
}

void MIDIOutput::sendFullFrame(const uint8_t* sysex10)
{
    sendRaw(sysex10, 10);
}

// ── Platform helpers ──────────────────────────────────────────────────────────

bool MIDIOutput::isLoopMIDIInstalled()
{
#ifdef _WIN32
    // loopMIDI registers a Windows service; its presence confirms the driver is installed.
    HKEY hKey = nullptr;
    LONG result = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                                "SYSTEM\\CurrentControlSet\\Services\\loopmidi",
                                0, KEY_READ, &hKey);
    if (result == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return true;
    }
    return false;
#else
    return true; // macOS uses a CoreMIDI virtual port — no driver needed
#endif
}

} // namespace StudioLog
