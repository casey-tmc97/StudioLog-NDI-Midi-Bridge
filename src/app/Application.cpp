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

namespace StudioLog {

Application::Application(int& argc, char** argv)
    : QApplication(argc, argv)
{
    setApplicationName("StudioLog NDI MIDI Bridge");
    setOrganizationName("Texas Music Cafe");
    setOrganizationDomain("texasmusiccafe.org");
    setApplicationVersion("0.1.0");

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
    // Stop threads in reverse dependency order
    // TODO: call stop() on each threaded subsystem
    PlatformInit::shutdown();
}

void Application::start()
{
    Logger::info("Application::start — bringing up subsystems");
    // TODO: open MIDI port from saved settings
    // TODO: start NDI discovery
    // TODO: start NDI receiver
    // TODO: start LTC decoder
    // TODO: start MTC generator
    mainWindow_->show();
    trayIcon_->show();
}

void Application::initPlatform()
{
    PlatformInit::init(); // timeBeginPeriod(1) on Windows
}

void Application::connectSignals()
{
    // TODO: connect NDIDiscovery::sourcesChanged   → MainWindow (QueuedConnection)
    // TODO: connect NDIReceiver::statusChanged      → AppState   (QueuedConnection)
    // TODO: connect LTCDecoder::frameDecoded        → MTCGenerator (QueuedConnection)
    // TODO: connect LTCDecoder::lockChanged         → AppState   (QueuedConnection)
    // TODO: connect AppState::stateChanged          → MainWindow (QueuedConnection)
    // TODO: connect MTCGenerator::timecodeUpdated   → MainWindow (QueuedConnection)
}

} // namespace StudioLog
