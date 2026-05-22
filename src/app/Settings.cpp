#include "Settings.h"
#include <QSettings>

namespace StudioLog {

class Settings::Impl {
public:
    QSettings qs;
};

Settings::Settings(QObject* parent)
    : QObject(parent), d_(std::make_unique<Impl>())
{}

Settings::~Settings() = default;

// ── NDI ──────────────────────────────────────────────────────────────────────

QString Settings::ndiSourceName() const {
    return d_->qs.value("ndi/sourceName", QString{}).toString();
}
void Settings::setNdiSourceName(const QString& name) {
    d_->qs.setValue("ndi/sourceName", name);
    emit ndiSourceNameChanged(name);
}

int Settings::ltcChannel() const {
    return d_->qs.value("ndi/ltcChannel", -1).toInt(); // -1 = Auto
}
void Settings::setLtcChannel(int ch) {
    d_->qs.setValue("ndi/ltcChannel", ch);
    emit ltcChannelChanged(ch);
}

// ── MIDI ─────────────────────────────────────────────────────────────────────

QString Settings::midiPortName() const {
    return d_->qs.value("midi/portName", QString{}).toString();
}
void Settings::setMidiPortName(const QString& name) {
    d_->qs.setValue("midi/portName", name);
    emit midiPortNameChanged(name);
}

// ── UI ───────────────────────────────────────────────────────────────────────

bool Settings::startMinimized() const {
    return d_->qs.value("ui/startMinimized", false).toBool();
}
void Settings::setStartMinimized(bool v) {
    d_->qs.setValue("ui/startMinimized", v);
}

bool Settings::minimizeToTray() const {
    return d_->qs.value("ui/minimizeToTray", true).toBool();
}
void Settings::setMinimizeToTray(bool v) {
    d_->qs.setValue("ui/minimizeToTray", v);
}

} // namespace StudioLog
