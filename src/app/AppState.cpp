#include "AppState.h"
#include "util/Logger.h"

namespace StudioLog {

AppState::AppState(QObject* parent)
    : QObject(parent)
{}

AppState::~AppState() = default;

State AppState::currentState() const { return current_; }

QString AppState::stateName(State s)
{
    switch (s) {
        case State::Idle:         return "Idle";
        case State::Connecting:   return "Connecting";
        case State::SearchingLTC: return "Searching LTC";
        case State::Locked:       return "Locked";
        case State::Freewheel:    return "Freewheel";
        case State::Reconnecting: return "Reconnecting";
        default:                  return "Unknown";
    }
}

void AppState::transition(State next)
{
    if (next == current_) return;
    Logger::info(QString("State: %1 → %2")
                     .arg(stateName(current_))
                     .arg(stateName(next)));
    State old = current_;
    current_  = next;
    emit stateChanged(current_, old);
}

void AppState::onNDISourceSelected()
{
    transition(State::Connecting);
}

void AppState::onNDIConnected()
{
    transition(State::SearchingLTC);
}

void AppState::onNDIDisconnected()
{
    transition(State::Reconnecting);
}

void AppState::onLTCLockAcquired()
{
    transition(State::Locked);
}

void AppState::onLTCLockLost()
{
    // Begin freewheel; a timer in MTCGenerator fires onFreewheelTimeout() after 2 s
    transition(State::Freewheel);
}

void AppState::onFreewheelTimeout()
{
    transition(State::SearchingLTC);
}

} // namespace StudioLog
