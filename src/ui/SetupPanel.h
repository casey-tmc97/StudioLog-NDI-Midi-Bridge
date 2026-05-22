#pragma once
#include <QWidget>
#include <QStringList>

namespace StudioLog {

/// Reusable setup panel widget for NDI source, MIDI port, and LTC channel.
///
/// Can be embedded inside MainWindow or shown as a standalone dialog.
/// Emits signals when the user makes a selection; caller persists to Settings.
class SetupPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SetupPanel(QWidget* parent = nullptr);
    ~SetupPanel() override;

    void setNDISources(const QStringList& sources);
    void setMIDIPorts(const QStringList& ports);
    void setCurrentNDISource(const QString& source);
    void setCurrentMIDIPort(const QString& port);
    void setCurrentLTCChannel(int channel); ///< -1=Auto, 0=Left, 1=Right

signals:
    void ndiSourceChanged(const QString& source);
    void midiPortChanged(const QString& port);
    void ltcChannelChanged(int channel);

private slots:
    void onNDIComboChanged(int index);
    void onMIDIComboChanged(int index);
    void onChannelComboChanged(int index);
    void onRefreshNDIClicked();
    void onRefreshMIDIClicked();

signals:
    void refreshNDIRequested();
    void refreshMIDIRequested();

private:
    class Impl;
    std::unique_ptr<Impl> d_;
};

} // namespace StudioLog
