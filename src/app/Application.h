#pragma once
#include <QApplication>
#include <memory>

namespace StudioLog {

class MainWindow;
class AppState;
class Settings;
class NDIDiscovery;
class NDIReceiver;
class LTCDecoder;
class MTCGenerator;
class MIDIOutput;
class TrayIcon;

/// Top-level application object.
///
/// Owns all major subsystems.  Constructed once in main(); calls exec() which
/// runs the Qt event loop.  Subsystems are started and wired together in start().
class Application : public QApplication
{
    Q_OBJECT

public:
    Application(int& argc, char** argv);
    ~Application() override;

    /// Wire subsystems, show the main window, and start background threads.
    void start();

private:
    void initPlatform();   ///< timeBeginPeriod(1) on Windows, nothing on macOS
    void connectSignals(); ///< QMetaObject wiring between subsystems and UI

    std::unique_ptr<Settings>     settings_;
    std::unique_ptr<AppState>     state_;
    std::unique_ptr<NDIDiscovery> ndiDiscovery_;
    std::unique_ptr<NDIReceiver>  ndiReceiver_;
    std::unique_ptr<LTCDecoder>   ltcDecoder_;
    std::unique_ptr<MTCGenerator> mtcGenerator_;
    std::unique_ptr<MIDIOutput>   midiOutput_;
    std::unique_ptr<MainWindow>   mainWindow_;
    std::unique_ptr<TrayIcon>     trayIcon_;
};

} // namespace StudioLog
