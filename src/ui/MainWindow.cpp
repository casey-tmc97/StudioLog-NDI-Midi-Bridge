#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "SetupPanel.h"
#include "app/Settings.h"
#include <QCloseEvent>

namespace StudioLog {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui_(new Ui::MainWindow)
{
    ui_->setupUi(this);
    setWindowTitle("StudioLog NDI MIDI Bridge");

    // TODO: connect internal UI signals to slots
}

MainWindow::~MainWindow()
{
    delete ui_;
}

void MainWindow::setSettings(Settings* settings)
{
    settings_ = settings;
}

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
    // TODO: populate NDI source combo box
    (void)sources;
}

void MainWindow::onMIDIPortsChanged(const QStringList& ports)
{
    // TODO: populate MIDI port combo box
    (void)ports;
}

void MainWindow::onStatusMessage(const QString& msg)
{
    // TODO: append to status log widget
    (void)msg;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // TODO: if minimizeToTray is set, hide instead of closing
    event->accept();
}

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

void MainWindow::onChannelChanged(int channel)
{
    if (settings_) settings_->setLtcChannel(channel);
    emit ltcChannelChanged(channel);
}

void MainWindow::updateStateDisplay(State state)
{
    // TODO: update status label color/text based on state
    (void)state;
}

void MainWindow::updateTimecodeDisplay(const SMPTETimecode& tc)
{
    // TODO: format hh:mm:ss:ff and update timecode LCD label
    (void)tc;
}

} // namespace StudioLog
