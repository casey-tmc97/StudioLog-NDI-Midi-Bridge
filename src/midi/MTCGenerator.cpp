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

/// Convert a timecode to an absolute frame count for discontinuity detection.
/// Drop-frame correction is omitted intentionally — the few-frame threshold
/// makes exact DF accounting unnecessary.
static int64_t tcToTotalFrames(const SMPTETimecode& tc) noexcept
{
    return (static_cast<int64_t>(tc.hours) * 3600LL
          + tc.minutes * 60LL
          + tc.seconds) * framesPerSecond(tc.fps)
          + tc.frames;
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

    // Log what we are about to send so the user can verify fps / fps-code
    // matches their DAW's session frame rate (Ardour/Livetrax checks this).
    {
        const char* fpsName = "30";
        switch (currentTC_.fps) {
            case FPS::FPS_23976:   fpsName = "23.976"; break;
            case FPS::FPS_24:      fpsName = "24";     break;
            case FPS::FPS_25:      fpsName = "25";     break;
            case FPS::FPS_2997DF:  fpsName = "29.97DF"; break;
            case FPS::FPS_2997NDF: fpsName = "29.97NDF"; break;
            case FPS::FPS_30:      fpsName = "30";     break;
        }
        const uint8_t code = fpsCode(currentTC_.fps);
        // MIDI MTC fps codes: 0=24, 1=25, 2=29.97DF, 3=30NDF
        const char* codeName = (code == 0) ? "24fps"
                             : (code == 1) ? "25fps"
                             : (code == 2) ? "29.97DF"
                             :               "30fps-NDF";
        Logger::info(QString("MTCGenerator: MTC running — LTC=%1fps, "
                             "MTC fps code=%2 (%3) — "
                             "verify your DAW session is set to %4")
                         .arg(fpsName).arg(code).arg(codeName).arg(codeName));
        if (output_ && output_->isOpen())
            Logger::info(QString("MTCGenerator: sending to MIDI port \"%1\"")
                             .arg(output_->currentPortName()));
        else
            Logger::warn("MTCGenerator: no MIDI port open — select a port in the UI");
    }

    sendFullFrame(currentTC_);
    qfPiece_ = 0;

    auto lastLTCUpdateMs = std::chrono::steady_clock::now();
    auto lastFullFrameMs = lastLTCUpdateMs; // tracks periodic Full Frame sends

    while (!stopRequested_.load(std::memory_order_relaxed)) {
        int64_t intervalNs = qfIntervalNs(currentTC_.fps);

        int64_t nowNs   = HighResTimer::nowNs();
        int64_t targetNs = nowNs + intervalNs;

        // Consume pending timecode update (from LTC thread).
        //
        // Key design: Normal LTC arrives once per frame (~33 ms at 30 fps) but
        // one QF cycle spans 8 messages × ~8.3 ms = ~66 ms.  Resetting qfPiece_
        // on every LTC frame meant the device only ever saw pieces 0–3 and could
        // never lock.
        //
        // Fix: refresh the freewheel timer on every update, but only apply the
        // new TC — and only resync — at a QF cycle boundary (piece 0).  This
        // keeps all 8 nibbles in a cycle consistent and lets the QF stream run
        // uninterrupted.  A Full-Frame resync is sent only on a genuine
        // discontinuity (fps change or jump > 4 frames).
        {
            std::lock_guard<std::mutex> lk(tcMutex_);
            if (tcUpdated_) {
                // Always refresh the freewheel timer — LTC is alive.
                lastLTCUpdateMs = std::chrono::steady_clock::now();

                // Apply the new TC only at the start of a fresh QF cycle so
                // that mid-cycle nibbles remain consistent.
                //
                // Critically: for NORMAL running do NOT override the internal
                // counter with the incoming LTC value.  The internal counter
                // advances by exactly 2 frames after every QF cycle (piece 7 →
                // advanceByTwo()).  Overriding it with each LTC frame — even
                // when the diff is small — causes the stream to jump backwards
                // or skip frames (LTC arrives every 1 frame; QF cycles cover 2
                // frames; so pendingTC_ is always 1 frame behind currentTC_
                // after advanceByTwo, producing a -1 snap every other cycle).
                //
                // Correct policy: let the internal counter free-run; only snap
                // it back to LTC on a genuine discontinuity (fps change or
                // jump > 4 frames).
                if (qfPiece_ == 0) {
                    const bool fpsMismatch  = (pendingTC_.fps != currentTC_.fps);
                    const int64_t frameDiff = std::abs(tcToTotalFrames(pendingTC_)
                                                     - tcToTotalFrames(currentTC_));
                    const bool discontinuous = fpsMismatch || (frameDiff > 4);

                    tcUpdated_ = false; // always consume the pending update

                    if (discontinuous) {
                        // Genuine jump or fps change — snap and resync receiver.
                        currentTC_ = pendingTC_;
                        Logger::info(QString("MTCGenerator: resync (jump=%1 frames, fpsChange=%2)")
                                         .arg(frameDiff).arg(fpsMismatch));
                        sendFullFrame(currentTC_);
                        lastFullFrameMs = std::chrono::steady_clock::now();
                        // qfPiece_ is already 0; QF stream continues.
                    }
                    // Otherwise: internal counter (advanceByTwo) keeps running.
                }
            }
        }

        // Periodic Full Frame every 10 s — lets a DAW (e.g. Livetrax/Ardour)
        // that connects mid-stream quickly locate the timecode position and lock
        // without waiting for the next discontinuity resync.
        // Only send at piece 0 so it doesn't interrupt a QF cycle.
        if (qfPiece_ == 0) {
            auto msSinceFF = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - lastFullFrameMs).count();
            if (msSinceFF >= 10'000) {
                sendFullFrame(currentTC_);
                lastFullFrameMs = std::chrono::steady_clock::now();
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
