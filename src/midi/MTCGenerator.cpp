#include "MTCGenerator.h"
#include "MIDIOutput.h"
#include "util/HighResTimer.h"
#include "util/Logger.h"
#include <QMetaObject>
#include <chrono>
#include <thread>

#ifdef _WIN32
#  include <windows.h>
#endif

namespace StudioLog {

MTCGenerator::MTCGenerator(QObject* parent)
    : QObject(parent)
{}

MTCGenerator::~MTCGenerator()
{
    stop();
}

void MTCGenerator::setMIDIOutput(MIDIOutput* output)
{
    output_ = output;
}

void MTCGenerator::start()
{
    stop();
    stopRequested_.store(false, std::memory_order_relaxed);
    running_.store(true, std::memory_order_relaxed);
    thread_ = std::thread(&MTCGenerator::threadFunc, this);
}

void MTCGenerator::stop()
{
    stopRequested_.store(true, std::memory_order_relaxed);
    tcCv_.notify_all();
    if (thread_.joinable()) thread_.join();
    running_.store(false, std::memory_order_relaxed);
}

void MTCGenerator::setTimecode(const SMPTETimecode& tc)
{
    {
        std::lock_guard<std::mutex> lk(tcMutex_);
        pendingTC_  = tc;
        tcUpdated_  = true;
    }
    tcCv_.notify_one();
}

void MTCGenerator::threadFunc()
{
#ifdef _WIN32
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
#endif

    Logger::info("MTCGenerator: thread started");

    // Wait for first timecode update
    {
        std::unique_lock<std::mutex> lk(tcMutex_);
        tcCv_.wait(lk, [this]{ return tcUpdated_ || stopRequested_.load(); });
        if (stopRequested_.load()) return;
        currentTC_ = pendingTC_;
        tcUpdated_ = false;
    }

    sendFullFrame(currentTC_);
    qfPiece_ = 0;

    auto lastLTCUpdateMs = std::chrono::steady_clock::now();

    while (!stopRequested_.load(std::memory_order_relaxed)) {
        int64_t intervalNs = qfIntervalNs(currentTC_.fps);

        // TODO: get current time in nanoseconds from HighResTimer
        int64_t nowNs   = HighResTimer::nowNs();
        int64_t targetNs = nowNs + intervalNs;

        // Consume pending timecode update (from LTC thread)
        {
            std::lock_guard<std::mutex> lk(tcMutex_);
            if (tcUpdated_) {
                currentTC_ = pendingTC_;
                tcUpdated_ = false;
                lastLTCUpdateMs = std::chrono::steady_clock::now();

                // Re-sync: send full frame and restart QF sequence
                sendFullFrame(currentTC_);
                qfPiece_ = 0;
            }
        }

        // Freewheel check
        auto msSinceLTC = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - lastLTCUpdateMs).count();
        if (msSinceLTC > FREEWHEEL_MS) {
            Logger::info("MTCGenerator: freewheel expired");
            QMetaObject::invokeMethod(this, [this]{ emit freewheelExpired(); },
                                      Qt::QueuedConnection);
            break;
        }

        // Build and send the next quarter-frame message
        auto qf = MTCQuarterFrame::make(qfPiece_, currentTC_);
        if (output_) output_->sendQuarterFrame(qf.data);

        if (qfPiece_ == 7) {
            currentTC_.advanceByTwo();
            QMetaObject::invokeMethod(this,
                [this, tc = currentTC_]{ emit timecodeUpdated(tc); },
                Qt::QueuedConnection);
        }
        qfPiece_ = (qfPiece_ + 1) & 0x07u;

        // Sleep-to-target with busy-wait tail
        busyWaitUntil(targetNs);
    }

    Logger::info("MTCGenerator: thread stopped");
}

void MTCGenerator::sendFullFrame(const SMPTETimecode& tc)
{
    MTCFullFrame ff(tc);
    if (output_) output_->sendFullFrame(ff.data);
}

void MTCGenerator::busyWaitUntil(int64_t targetNs)
{
    // Sleep most of the interval, busy-wait the last ~0.5 ms tail
    constexpr int64_t BUSY_TAIL_NS = 500'000LL;
    int64_t sleepUntil = targetNs - BUSY_TAIL_NS;

    int64_t now = HighResTimer::nowNs();
    if (now < sleepUntil) {
        // TODO: use std::this_thread::sleep_until with a high-res clock
        int64_t sleepNs = sleepUntil - now;
        std::this_thread::sleep_for(std::chrono::nanoseconds(sleepNs));
    }

    // Busy-wait
    while (HighResTimer::nowNs() < targetNs) {
        // spin
    }
}

} // namespace StudioLog
