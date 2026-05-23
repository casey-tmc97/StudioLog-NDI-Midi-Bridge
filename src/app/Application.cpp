#include "Application.h"
#include "AppState.h"
#include "Settings.h"
#include "ndi/NDIDiscovery.h"
#include "ndi/NDIReceiver.h"
#include "ltc/LTCDecoder.h"
#include "midi/MTCGenerator.h"
#include "midi/MIDIOutput.h"
#include "ui/MainWindow.h"
#include "ui/TrayIcon.h"
#include "util/PlatformInit.h"
#include "util/Logger.h"
#include <QMetaObject>
#include <QMetaType>

namespace StudioLog {

// ── Construction / destruction ────────────────────────────────────────────────

Application::Application(int& argc, char** argv)
    : QApplication(argc, argv)
{
    setApplicationName("StudioLog NDI MIDI Bridge");
    setOrganizationName("Texas Music Cafe");
    setOrganizationDomain("texasmusiccafe.org");
    setApplicationVersion("0.1.0");

    // Required for Qt::QueuedConnection to serialise SMPTETimecode across the
    // event loop (frameDecoded → setTimecode, timecodeUpdated → UI update).
    qRegisterMetaType<StudioLog::SMPTETimecode>("StudioLog::SMPTETimecode");

    initPlatform();

    settings_     = std::make_unique<Settings>();
    state_        = std::make_unique<AppState>();
    ndiDiscovery_ = std::make_unique<NDIDiscovery>();
    ndiReceiver_  = std::make_unique<NDIReceiver>();
    ltcDecoder_   = std::make_unique<LTCDecoder>();
    mtcGenerator_ = std::make_unique<MTCGenerator>();
    midiOutput_   = std::make_unique<MIDIOutput>();
    mainWindow_   = std::make_unique<MainWindow>();
    trayIcon_     = std::make_unique<TrayIcon>(mainWindow_.get());

    connectSignals();
    start();
}

Application::~Application()
{
    // Stop threads in reverse dependency order so no object reads from a
    // destroyed neighbour.  MTCGenerator sends via MIDIOutput, so it stops
    // first; LTCDecoder reads from NDIReceiver's ring buffer, so it stops
    // before NDIReceiver; NDIDiscovery is independent but uses the same NDI
    // library, so it stops last.
    Logger::info("Application: shutting down");
    Logger::setCallback({}); // prevent dangling pointer after mainWindow_ destructs

    if (mtcGenerator_) mtcGenerator_->stop();
    if (ltcDecoder_)   ltcDecoder_->stop();
    if (ndiReceiver_)  ndiReceiver_->disconnect();
    if (ndiDiscovery_) ndiDiscovery_->stop();

    PlatformInit::shutdown();
}

// ── Platform init ─────────────────────────────────────────────────────────────

void Application::initPlatform()
{
    PlatformInit::init(); // timeBeginPeriod(1) on Windows
}

// ── Signal wiring ─────────────────────────────────────────────────────────────

void Application::connectSignals()
{
    // ─── NDI Discovery → MainWindow ──────────────────────────────────────────
    // Delivers the current source list on every 3-second poll change.
    connect(ndiDiscovery_.get(), &NDIDiscovery::sourcesChanged,
            mainWindow_.get(),  &MainWindow::onNDISourcesChanged,
            Qt::QueuedConnection);

    // ─── NDI Receiver → AppState ─────────────────────────────────────────────
    // NDIReceiver::connected carries the resolved source name; AppState just
    // needs the event, so a lambda drops the argument.
    connect(ndiReceiver_.get(), &NDIReceiver::connected,
            state_.get(), [this](const QString&) { state_->onNDIConnected(); },
            Qt::QueuedConnection);

    connect(ndiReceiver_.get(), &NDIReceiver::disconnected,
            state_.get(), &AppState::onNDIDisconnected,
            Qt::QueuedConnection);

    // ─── LTC Decoder → AppState ──────────────────────────────────────────────
    // lockChanged(true)  → lock acquired; lockChanged(false) → freewheel begins
    connect(ltcDecoder_.get(), &LTCDecoder::lockChanged,
            state_.get(), [this](bool locked) {
                if (locked) state_->onLTCLockAcquired();
                else        state_->onLTCLockLost();
            }, Qt::QueuedConnection);

    // ─── LTC Decoder → MTC Generator (timecode feed, thread-safe) ───────────
    // DirectConnection: frameDecoded is emitted from the LTC decode thread;
    // setTimecode() is mutex-protected so direct cross-thread delivery is safe
    // and eliminates the two-hop event-loop latency that caused resync bursts.
    connect(ltcDecoder_.get(), &LTCDecoder::frameDecoded,
            mtcGenerator_.get(), &MTCGenerator::setTimecode,
            Qt::DirectConnection);

    // ─── LTC lock → start MTC Generator ──────────────────────────────────────
    // Start the QF output thread on lock acquisition.  On lock loss, the thread
    // keeps freewheeling; freewheelExpired stops it.
    connect(ltcDecoder_.get(), &LTCDecoder::lockChanged,
            this, [this](bool locked) {
                if (locked) mtcGenerator_->start();
            }, Qt::QueuedConnection);

    // ─── MTC Generator → AppState (freewheel expiry) ─────────────────────────
    connect(mtcGenerator_.get(), &MTCGenerator::freewheelExpired,
            state_.get(), &AppState::onFreewheelTimeout,
            Qt::QueuedConnection);

    // Stop the MTC thread after freewheel expires (thread has already exited by
    // this point; stop() just joins and resets the running flag).
    connect(mtcGenerator_.get(), &MTCGenerator::freewheelExpired,
            mtcGenerator_.get(), &MTCGenerator::stop,
            Qt::QueuedConnection);

    // ─── MTC Generator → MainWindow (live timecode display) ─────────────────
    connect(mtcGenerator_.get(), &MTCGenerator::timecodeUpdated,
            mainWindow_.get(), &MainWindow::onTimecodeUpdated,
            Qt::QueuedConnection);

    // ─── AppState → MainWindow (status display) ───────────────────────────────
    connect(state_.get(), &AppState::stateChanged,
            mainWindow_.get(), &MainWindow::onStateChanged,
            Qt::QueuedConnection);

    // ─── MIDI Output → MainWindow (port list) ────────────────────────────────
    connect(midiOutput_.get(), &MIDIOutput::portsChanged,
            mainWindow_.get(), &MainWindow::onMIDIPortsChanged,
            Qt::QueuedConnection);

    // ─── MainWindow → Application (user actions) ──────────────────────────────

    // NDI source selected by the user
    connect(mainWindow_.get(), &MainWindow::ndiSourceSelected,
            this, [this](const QString& source) {
                settings_->setNdiSourceName(source);
                state_->onNDISourceSelected(); // → Connecting

                // Stop any existing decode pipeline before reconnecting
                mtcGenerator_->stop();
                ltcDecoder_->stop();

                // Re-launch the receive → decode pipeline with the new source
                ndiReceiver_->connectToSource(source, settings_->ltcChannel());
                ltcDecoder_->start(ndiReceiver_->ringBuffer());
            });

    // MIDI port selected by the user
    connect(mainWindow_.get(), &MainWindow::midiPortSelected,
            this, [this](const QString& port) {
                settings_->setMidiPortName(port);
                midiOutput_->openPort(port);
            });

    // LTC channel changed (Left / Right / Auto)
    connect(mainWindow_.get(), &MainWindow::ltcChannelChanged,
            this, [this](int ch) {
                settings_->setLtcChannel(ch);
                // Reconnect with new channel if a source is already active
                const QString src = settings_->ndiSourceName();
                if (!src.isEmpty() && ndiReceiver_->isRunning()) {
                    ltcDecoder_->stop();
                    ndiReceiver_->connectToSource(src, ch);
                    ltcDecoder_->start(ndiReceiver_->ringBuffer());
                }
            });
}

// ── Startup sequence ──────────────────────────────────────────────────────────

void Application::start()
{
    Logger::info("Application::start — bringing up subsystems");

    // Route log messages to the UI log view (Info and above only).
    // The callback fires on whatever thread called Logger::log(); we marshal
    // to the main thread via QueuedConnection so the UI is never touched from
    // a background thread.
    Logger::setCallback([this](Logger::Level level, const QString& line) {
        if (level >= Logger::Level::Info) {
            QMetaObject::invokeMethod(mainWindow_.get(),
                [mw = mainWindow_.get(), line]{ mw->onStatusMessage(line); },
                Qt::QueuedConnection);
        }
    });

    // Give the main window access to Settings so it can pre-populate UI fields
    mainWindow_->setSettings(settings_.get());

    // Wire the MTC generator to the MIDI output before either is started
    mtcGenerator_->setMIDIOutput(midiOutput_.get());

    // ── MIDI port ─────────────────────────────────────────────────────────────
    // Populate the port list in the UI regardless of whether a saved port exists
    mainWindow_->onMIDIPortsChanged(midiOutput_->availablePorts());

    const QString savedPort = settings_->midiPortName();
    if (!savedPort.isEmpty()) {
        if (!midiOutput_->openPort(savedPort)) {
            Logger::warn(QString("MIDIOutput: saved port \"%1\" not available — "
                                 "select one from the MIDI menu").arg(savedPort));
        }
    }

    // Poll for MIDI port changes every 3 s so loopMIDI ports added at runtime
    // appear in the UI without restarting the app.
    midiOutput_->startPortPolling(3000);

#ifdef _WIN32
    if (!MIDIOutput::isLoopMIDIInstalled()) {
        Logger::warn("loopMIDI does not appear to be installed.  "
                     "Install loopMIDI and create a virtual port, then "
                     "select it from the MIDI port menu.");
    }
#endif

    // ── NDI Discovery ─────────────────────────────────────────────────────────
    ndiDiscovery_->start(); // begins polling every 3 s; emits sourcesChanged immediately

    // ── Restore last NDI source ───────────────────────────────────────────────
    const QString savedNdi = settings_->ndiSourceName();
    if (!savedNdi.isEmpty()) {
        Logger::info(QString("Application: restoring NDI source \"%1\"").arg(savedNdi));
        state_->onNDISourceSelected(); // → Connecting
        ndiReceiver_->connectToSource(savedNdi, settings_->ltcChannel());
        ltcDecoder_->start(ndiReceiver_->ringBuffer());
    }

    // ── Show UI ───────────────────────────────────────────────────────────────
    if (settings_->startMinimized()) {
        trayIcon_->show();
        // mainWindow_ stays hidden; tray icon is the only entry point
    } else {
        mainWindow_->show();
        trayIcon_->show();
    }
}

} // namespace StudioLog
