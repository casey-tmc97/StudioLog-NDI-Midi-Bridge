#pragma once
#include "app/AppState.h"
#include <QSystemTrayIcon>
#include <QMenu>

namespace StudioLog {

/// System tray icon with context menu.
///
/// Shows a status indicator that reflects the current AppState.
/// Double-clicking restores the main window.
class TrayIcon : public QSystemTrayIcon
{
    Q_OBJECT

public:
    explicit TrayIcon(QWidget* mainWindow, QObject* parent = nullptr);
    ~TrayIcon() override;

public slots:
    void onStateChanged(StudioLog::State newState, StudioLog::State oldState);
    /// Shows a one-time balloon telling the user how to fully quit.
    void showMinimizeHint();

private slots:
    void onActivated(QSystemTrayIcon::ActivationReason reason);
    void onShowAction();
    void onQuitAction();

private:
    void buildContextMenu();
    void updateIcon(StudioLog::State state);

    QWidget* mainWindow_ = nullptr;
    QMenu    contextMenu_;
};

} // namespace StudioLog
