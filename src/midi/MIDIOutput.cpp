#include "MIDIOutput.h"
#include "util/Logger.h"
#include <rtmidi/RtMidi.h>
#include <vector>

#ifdef _WIN32
#  include <windows.h>
#endif

namespace StudioLog {

MIDIOutput::MIDIOutput(QObject* parent)
    : QObject(parent)
{
    // TODO: construct RtMidiOut with preferred API
    //   midi_ = std::make_unique<RtMidiOut>(RtMidi::UNSPECIFIED, "StudioLog MTC");
}

MIDIOutput::~MIDIOutput()
{
    closePort();
}

QStringList MIDIOutput::availablePorts() const
{
    QStringList names;
    if (!midi_) return names;
    // TODO:
    //   unsigned int count = midi_->getPortCount();
    //   for (unsigned i = 0; i < count; ++i)
    //       names << QString::fromStdString(midi_->getPortName(i));
    return names;
}

bool MIDIOutput::openPort(const QString& portName)
{
    closePort();
    if (!midi_) return false;

    QStringList ports = availablePorts();
    for (int i = 0; i < ports.size(); ++i) {
        if (ports[i].contains(portName, Qt::CaseInsensitive)) {
            // TODO: midi_->openPort(static_cast<unsigned>(i), "StudioLog MTC Out");
            portOpen_       = true;
            currentPortName_ = ports[i];
            Logger::info("MIDIOutput: opened port \"" + currentPortName_ + "\"");
            emit portOpened(currentPortName_);
            return true;
        }
    }
    Logger::warn("MIDIOutput: port not found: " + portName);
    return false;
}

void MIDIOutput::closePort()
{
    if (!portOpen_) return;
    // TODO: midi_->closePort();
    portOpen_       = false;
    currentPortName_.clear();
    emit portClosed();
}

void MIDIOutput::sendRaw(const uint8_t* data, std::size_t len)
{
    if (!portOpen_ || !midi_) return;
    // TODO: midi_->sendMessage(data, len);
    (void)data; (void)len;
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

bool MIDIOutput::isLoopMIDIInstalled()
{
#ifdef _WIN32
    // loopMIDI installs a Windows service; check for its registry key.
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
    return true; // macOS uses CoreMIDI virtual port — always available
#endif
}

} // namespace StudioLog
