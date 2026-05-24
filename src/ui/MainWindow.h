#pragma once
#include "app/AppState.h"
#include "midi/MTCTypes.h"
#include <QMainWindow>
#include <QStringList>
#include <QString>

namespace Ui { class MainWindow; }

namespace StudioLog {

class Settings;
class SetupPanel;

/// Main application window.
///
/// Displays:
///   - Current app state (Idle / Connecting / Searching LTC / Locked / Freewheel)
///   - Live timecode readout when locked
///   - LTC channel indicator (Left / Right / Auto)
///   - NDI source selector → SetupPanel
///   - MIDI port selector   → SetupPanel
///   - Log/status area
///
/// All slot calls arrive on the main thread via QueuedConnection.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void setSettings(Settings* settings);

    /// Set the log file path shown by Help > Show Log File.
    void setLogPath(const QString& path);

public slots:
    void onStateChanged(StudioLog::State newState, StudioLog::State oldState);
    void onTimecodeUpdated(StudioLog::SMPTETimecode tc);
    void onNDISourcesChanged(const QStringList& sources);
    void onMIDIPortsChanged(const QStringList& ports);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onNDISourceSelected(const QString& source);
    void onMIDIPortSelected(const QString& port);
    void onChannelChanged(int channel);

signals:
    void ndiSourceSelected(const QString& source);
    void midiPortSelected(const QString& port);
    void ltcChannelChanged(int channel);

private:
    void updateStateDisplay(StudioLog::State state);
    void updateTimecodeDisplay(const StudioLog::SMPTETimecode& tc);

    Ui::MainWindow* ui_ = nullptr;
    Settings*       settings_ = nullptr;
    QString         logPath_;
};

} // namespace StudioLog
