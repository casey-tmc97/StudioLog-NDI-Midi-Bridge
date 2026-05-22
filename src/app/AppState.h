#pragma once
#include <QObject>

namespace StudioLog {

/// Application-level state machine.
///
/// Transitions are driven by signals from NDIReceiver and LTCDecoder.
/// All transitions must occur on the main thread (use QueuedConnection).
enum class State {
    Idle,           ///< No NDI source selected / not started
    Connecting,     ///< NDI source selected, establishing receive
    SearchingLTC,   ///< NDI audio flowing, waiting for LTC lock
    Locked,         ///< LTC locked; MTC output active
    Freewheel,      ///< LTC dropout; MTC continuing for up to 2 s
    Reconnecting,   ///< NDI connection lost; attempting to reconnect
};

class AppState : public QObject
{
    Q_OBJECT

public:
    explicit AppState(QObject* parent = nullptr);
    ~AppState() override;

    State currentState() const;
    static QString stateName(State s);

public slots:
    void onNDIConnected();
    void onNDIDisconnected();
    void onLTCLockAcquired();
    void onLTCLockLost();
    void onFreewheelTimeout();

signals:
    void stateChanged(State newState, State oldState);

private:
    void transition(State next);

    State current_ = State::Idle;
};

} // namespace StudioLog
