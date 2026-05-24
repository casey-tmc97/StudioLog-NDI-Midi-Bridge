#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "app/Settings.h"
#include <QCloseEvent>
#include <QShowEvent>
#include <QDesktopServices>
#include <QMessageBox>
#include <QApplication>
#include <QSystemTrayIcon>
#include <QUrl>

namespace StudioLog {

// ── Construction ──────────────────────────────────────────────────────────────

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui_(new Ui::MainWindow)
{
    ui_->setupUi(this);
    setWindowTitle("StudioLog NDI MIDI Bridge");


    // ── Internal combo-box → private slot wiring ──────────────────────────────
    // Use activated() not currentIndexChanged() so programmatic updates
    // (via QSignalBlocker in the on*Changed slots) don't fire user-action signals.
    connect(ui_->ndiCombo,  QOverload<int>::of(&QComboBox::activated),
            this, [this](int /*idx*/) {
                onNDISourceSelected(ui_->ndiCombo->currentText());
            });

    connect(ui_->midiCombo, QOverload<int>::of(&QComboBox::activated),
            this, [this](int /*idx*/) {
                onMIDIPortSelected(ui_->midiCombo->currentText());
            });

    connect(ui_->channelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onChannelChanged);

    // ── Menu actions ──────────────────────────────────────────────────────────
    connect(ui_->actionQuit, &QAction::triggered, qApp, &QApplication::quit);

    connect(ui_->actionShowLog, &QAction::triggered, this, [this] {
        if (logPath_.isEmpty()) {
            QMessageBox::information(this, "Log File", "No log file has been configured.");
            return;
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(logPath_));
    });

    connect(ui_->actionAbout, &QAction::triggered, this, [this] {
        QMessageBox::about(this,
            "About StudioLog NDI MIDI Bridge",
            "<b>StudioLog NDI MIDI Bridge</b> v" + QApplication::applicationVersion() +
            "<br><br>"
            "Receives SMPTE LTC embedded in an NDI audio stream and<br>"
            "converts it to MIDI Timecode (MTC) in real time.<br><br>"
            "Texas Music Cafe · <a href='https://texasmusiccafe.org'>texasmusiccafe.org</a><br>"
            "501(c)(3) nonprofit live broadcast");
    });

    // Initial display state
    updateStateDisplay(State::Idle);
}

MainWindow::~MainWindow()
{
    delete ui_;
}

// ── Settings pre-population ───────────────────────────────────────────────────

void MainWindow::setSettings(Settings* settings)
{
    settings_ = settings;
    if (!settings_) return;

    // Pre-select the saved LTC channel (lists are not yet populated here;
    // NDI/MIDI selections are restored inside on*Changed when lists arrive).
    const int ch = settings_->ltcChannel(); // -1=Auto, 0=Left, 1=Right
    QSignalBlocker b(ui_->channelCombo);
    ui_->channelCombo->setCurrentIndex(ch + 1); // -1→0, 0→1, 1→2
}

// ── Public slots (called from Application's signal connections) ───────────────

void MainWindow::onStateChanged(State newState, State /*oldState*/)
{
    updateStateDisplay(newState);
}

void MainWindow::onTimecodeUpdated(SMPTETimecode tc)
{
    updateTimecodeDisplay(tc);
}

void MainWindow::onNDISourcesChanged(const QStringList& sources)
{
    QSignalBlocker b(ui_->ndiCombo);
    const QString current = ui_->ndiCombo->currentText();

    ui_->ndiCombo->clear();
    ui_->ndiCombo->addItems(sources);

    // Restore selection: prefer what was already shown, then fall back to saved
    int idx = -1;
    if (!current.isEmpty())
        idx = ui_->ndiCombo->findText(current);
    if (idx < 0 && settings_)
        idx = ui_->ndiCombo->findText(settings_->ndiSourceName());
    if (idx >= 0)
        ui_->ndiCombo->setCurrentIndex(idx);
}

void MainWindow::onMIDIPortsChanged(const QStringList& ports)
{
    QSignalBlocker b(ui_->midiCombo);
    const QString current = ui_->midiCombo->currentText();

    ui_->midiCombo->clear();
    ui_->midiCombo->addItems(ports);

    // Restore selection: prefer what was shown, then saved port name (partial match)
    int idx = -1;
    if (!current.isEmpty())
        idx = ui_->midiCombo->findText(current);
    if (idx < 0 && settings_)
        idx = ui_->midiCombo->findText(settings_->midiPortName(), Qt::MatchContains);
    if (idx >= 0)
        ui_->midiCombo->setCurrentIndex(idx);
}

void MainWindow::setLogPath(const QString& path)
{
    logPath_ = path;
}

// ── Show / close ──────────────────────────────────────────────────────────────

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    // Lock window size to its content on first show.  adjustSize() is called
    // here (not in the constructor) because the layout's sizeHint() is only
    // fully computed after the first layout pass, which happens at show time.
    if (!m_sizeLocked) {
        m_sizeLocked = true;
        adjustSize();
        setFixedSize(size());
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        // Always minimize to tray — File > Quit is the only way to exit.
        hide();
        event->ignore();
        if (!m_trayHintSent) {
            m_trayHintSent = true;
            emit minimizedToTray();
        }
    } else {
        // No system tray: fall back to a real quit.
        event->accept();
        QApplication::quit();
    }
}

// ── Private slots (from combo boxes) ─────────────────────────────────────────

void MainWindow::onNDISourceSelected(const QString& source)
{
    if (settings_) settings_->setNdiSourceName(source);
    emit ndiSourceSelected(source);
}

void MainWindow::onMIDIPortSelected(const QString& port)
{
    if (settings_) settings_->setMidiPortName(port);
    emit midiPortSelected(port);
}

void MainWindow::onChannelChanged(int index)
{
    // Combo indices: 0 = Auto (-1), 1 = Left (0), 2 = Right (1)
    const int channel = index - 1;
    if (settings_) settings_->setLtcChannel(channel);
    emit ltcChannelChanged(channel);
}

// ── Display helpers ───────────────────────────────────────────────────────────

void MainWindow::updateStateDisplay(State state)
{
    struct { const char* text; const char* style; } info;

    switch (state) {
    case State::Idle:
        info = { "Idle",
                 "color: #888888; font-weight: normal;" };
        break;
    case State::Connecting:
        info = { "Connecting…",
                 "color: #5588cc; font-weight: normal;" };
        break;
    case State::SearchingLTC:
        info = { "Searching for LTC…",
                 "color: #cc8800; font-weight: normal;" };
        break;
    case State::Locked:
        info = { "● Locked",
                 "color: #22aa44; font-weight: bold;" };
        break;
    case State::Freewheel:
        info = { "◐ Freewheeling",
                 "color: #cc6600; font-weight: bold;" };
        break;
    case State::Reconnecting:
        info = { "Reconnecting…",
                 "color: #cc2222; font-weight: normal;" };
        break;
    default:
        info = { "Unknown", "color: #888888;" };
        break;
    }

    ui_->stateLabel->setText(info.text);
    ui_->stateLabel->setStyleSheet(QString("QLabel { %1 }").arg(info.style));

    // Grey out the timecode when not locked
    if (state != State::Locked && state != State::Freewheel) {
        ui_->timecodeLabel->setStyleSheet("QLabel { color: #888888; }");
        ui_->timecodeLabel->setText("--:--:--:--");
    } else if (state == State::Locked) {
        ui_->timecodeLabel->setStyleSheet("QLabel { color: #22aa44; }");
    } else { // Freewheel
        ui_->timecodeLabel->setStyleSheet("QLabel { color: #cc6600; }");
    }
}

void MainWindow::updateTimecodeDisplay(const SMPTETimecode& tc)
{
    const QString text = QString("%1:%2:%3:%4")
        .arg(tc.hours,   2, 10, QChar('0'))
        .arg(tc.minutes, 2, 10, QChar('0'))
        .arg(tc.seconds, 2, 10, QChar('0'))
        .arg(tc.frames,  2, 10, QChar('0'));

    ui_->timecodeLabel->setText(text);

    // Show the frame-rate and drop-frame flag in the status bar
    const char* fpsStr = "30";
    switch (tc.fps) {
    case FPS::FPS_23976:   fpsStr = "23.976"; break;
    case FPS::FPS_24:      fpsStr = "24";     break;
    case FPS::FPS_25:      fpsStr = "25";     break;
    case FPS::FPS_2997DF:  fpsStr = "29.97DF"; break;
    case FPS::FPS_2997NDF: fpsStr = "29.97";  break;
    case FPS::FPS_30:      fpsStr = "30";     break;
    }
    statusBar()->showMessage(QString("%1 fps").arg(fpsStr));
}

} // namespace StudioLog
