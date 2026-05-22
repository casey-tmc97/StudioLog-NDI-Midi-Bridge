#include "SetupPanel.h"
#include <QComboBox>
#include <QFormLayout>
#include <QPushButton>
#include <QHBoxLayout>

namespace StudioLog {

class SetupPanel::Impl {
public:
    QComboBox*    ndiCombo     = nullptr;
    QComboBox*    midiCombo    = nullptr;
    QComboBox*    channelCombo = nullptr;
    QPushButton*  refreshNdi   = nullptr;
    QPushButton*  refreshMidi  = nullptr;
};

SetupPanel::SetupPanel(QWidget* parent)
    : QWidget(parent)
    , d_(std::make_unique<Impl>())
{
    d_->ndiCombo     = new QComboBox(this);
    d_->midiCombo    = new QComboBox(this);
    d_->channelCombo = new QComboBox(this);
    d_->refreshNdi   = new QPushButton("↻", this);
    d_->refreshMidi  = new QPushButton("↻", this);

    d_->channelCombo->addItems({"Auto-detect", "Left (Ch 1)", "Right (Ch 2)"});

    auto* ndiRow  = new QHBoxLayout();
    ndiRow->addWidget(d_->ndiCombo, 1);
    ndiRow->addWidget(d_->refreshNdi);

    auto* midiRow = new QHBoxLayout();
    midiRow->addWidget(d_->midiCombo, 1);
    midiRow->addWidget(d_->refreshMidi);

    auto* form = new QFormLayout(this);
    form->addRow("NDI Source:", ndiRow);
    form->addRow("MIDI Port:",  midiRow);
    form->addRow("LTC Channel:", d_->channelCombo);

    connect(d_->ndiCombo,     QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SetupPanel::onNDIComboChanged);
    connect(d_->midiCombo,    QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SetupPanel::onMIDIComboChanged);
    connect(d_->channelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SetupPanel::onChannelComboChanged);
    connect(d_->refreshNdi,   &QPushButton::clicked, this, &SetupPanel::onRefreshNDIClicked);
    connect(d_->refreshMidi,  &QPushButton::clicked, this, &SetupPanel::onRefreshMIDIClicked);
}

SetupPanel::~SetupPanel() = default;

void SetupPanel::setNDISources(const QStringList& sources)
{
    QSignalBlocker b(d_->ndiCombo);
    QString cur = d_->ndiCombo->currentText();
    d_->ndiCombo->clear();
    d_->ndiCombo->addItems(sources);
    int idx = d_->ndiCombo->findText(cur);
    if (idx >= 0) d_->ndiCombo->setCurrentIndex(idx);
}

void SetupPanel::setMIDIPorts(const QStringList& ports)
{
    QSignalBlocker b(d_->midiCombo);
    QString cur = d_->midiCombo->currentText();
    d_->midiCombo->clear();
    d_->midiCombo->addItems(ports);
    int idx = d_->midiCombo->findText(cur);
    if (idx >= 0) d_->midiCombo->setCurrentIndex(idx);
}

void SetupPanel::setCurrentNDISource(const QString& source)
{
    QSignalBlocker b(d_->ndiCombo);
    int idx = d_->ndiCombo->findText(source);
    if (idx >= 0) d_->ndiCombo->setCurrentIndex(idx);
}

void SetupPanel::setCurrentMIDIPort(const QString& port)
{
    QSignalBlocker b(d_->midiCombo);
    int idx = d_->midiCombo->findText(port, Qt::MatchContains);
    if (idx >= 0) d_->midiCombo->setCurrentIndex(idx);
}

void SetupPanel::setCurrentLTCChannel(int channel)
{
    QSignalBlocker b(d_->channelCombo);
    // -1=Auto→0, 0=Left→1, 1=Right→2
    d_->channelCombo->setCurrentIndex(channel + 1);
}

void SetupPanel::onNDIComboChanged(int /*index*/)
{
    emit ndiSourceChanged(d_->ndiCombo->currentText());
}

void SetupPanel::onMIDIComboChanged(int /*index*/)
{
    emit midiPortChanged(d_->midiCombo->currentText());
}

void SetupPanel::onChannelComboChanged(int index)
{
    emit ltcChannelChanged(index - 1); // 0→-1, 1→0, 2→1
}

void SetupPanel::onRefreshNDIClicked()  { emit refreshNDIRequested();  }
void SetupPanel::onRefreshMIDIClicked() { emit refreshMIDIRequested(); }

} // namespace StudioLog
