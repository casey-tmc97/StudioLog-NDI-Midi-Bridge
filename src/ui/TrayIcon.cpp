#include "TrayIcon.h"
#include <QApplication>
#include <QIcon>

namespace StudioLog {

TrayIcon::TrayIcon(QWidget* mainWindow, QObject* parent)
    : QSystemTrayIcon(parent)
    , mainWindow_(mainWindow)
{
    // TODO: setIcon(QIcon(":/icons/tray_idle.png"));
    setToolTip("StudioLog NDI MIDI Bridge");
    buildContextMenu();
    connect(this, &QSystemTrayIcon::activated, this, &TrayIcon::onActivated);
}

TrayIcon::~TrayIcon() = default;

void TrayIcon::buildContextMenu()
{
    QAction* showAction = contextMenu_.addAction("Show");
    contextMenu_.addSeparator();
    QAction* quitAction = contextMenu_.addAction("Quit");

    connect(showAction, &QAction::triggered, this, &TrayIcon::onShowAction);
    connect(quitAction, &QAction::triggered, this, &TrayIcon::onQuitAction);

    setContextMenu(&contextMenu_);
}

void TrayIcon::onStateChanged(State newState, State /*oldState*/)
{
    updateIcon(newState);
}

void TrayIcon::onActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick) {
        onShowAction();
    }
}

void TrayIcon::onShowAction()
{
    if (mainWindow_) {
        mainWindow_->show();
        mainWindow_->raise();
        mainWindow_->activateWindow();
    }
}

void TrayIcon::onQuitAction()
{
    QApplication::quit();
}

void TrayIcon::updateIcon(State state)
{
    // TODO: switch icon based on state
    //   Idle/Connecting/Searching → grey
    //   Locked                   → green
    //   Freewheel                → yellow
    //   Reconnecting             → red
    (void)state;
}

} // namespace StudioLog
