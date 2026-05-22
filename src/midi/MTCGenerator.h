#pragma once
#include "MTCTypes.h"
#include <QObject>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace StudioLog {

class MIDIOutput;

/// Outputs MIDI Timecode on a TIME_CRITICAL thread.
///
/// Protocol:
///   1. On lock acquisition: send Full Frame SysEx, then begin QF stream.
///   2. QF stream: 8 messages per 2-frame cycle at qfIntervalNs(fps) per message.
///   3. Timing: sleep_until target - ε, then busy-wait until target nanosecond.
///   4. Freewheel: continue running for up to FREEWHEEL_MS ms after last update.
///      After that, stop sending and emit freewheelExpired().
///
/// Thread safety:
///   - setTimecode() is called from the LTC decode thread via atomic + condvar.
///   - All public non-thread-safe methods must be called from the main thread.
class MTCGenerator : public QObject
{
    Q_OBJECT

public:
    explicit MTCGenerator(QObject* parent = nullptr);
    ~MTCGenerator() override;

    void setMIDIOutput(MIDIOutput* output);

    /// Start the QF output thread.
    void start();

    /// Stop the QF output thread.
    void stop();

    /// Update the current timecode from the LTC decode thread.
    /// Thread-safe.
    void setTimecode(const SMPTETimecode& tc);

    bool isRunning() const { return running_.load(std::memory_order_relaxed); }

    static constexpr int FREEWHEEL_MS = 2000;

signals:
    /// Emitted (queued) when freewheel expires with no new LTC frames.
    void freewheelExpired();

    /// Emitted (queued) each time the displayed timecode ticks.
    void timecodeUpdated(StudioLog::SMPTETimecode tc);

private:
    void threadFunc();
    void sendFullFrame(const SMPTETimecode& tc);
    void busyWaitUntil(int64_t targetNs);

    MIDIOutput* output_ = nullptr;

    std::thread            thread_;
    std::atomic<bool>      running_{false};
    std::atomic<bool>      stopRequested_{false};

    std::mutex             tcMutex_;
    std::condition_variable tcCv_;
    SMPTETimecode          pendingTC_;
    bool                   tcUpdated_ = false;

    // Working state inside the thread (no external access needed)
    SMPTETimecode          currentTC_;
    uint8_t                qfPiece_ = 0;
};

} // namespace StudioLog
