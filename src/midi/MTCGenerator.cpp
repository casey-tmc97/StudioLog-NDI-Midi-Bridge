#include "MTCGenerator.h"
#include "MIDIOutput.h"
#include "util/HighResTimer.h"
#include "util/Logger.h"
#include <QMetaObject>
#include <chrono>

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
        //
        // IMPORTANT: hold tcMutex_ only for the read + clear — NOT for the
        // subsequent Logger::info / sendFullFrame.  Those operations can block
        // (MIDI I/O, Logger callback) and would stall the LTC decode thread
        // calling setTimecode() via DirectConnection, causing ring-buffer
        // accumulation and burst delivery that looks like a large TC jump.
        //
        // The freewheel timer is refreshed at piece-0 only (every 66.7 ms),
        // which is well within the 2-second freewheel window.
        bool doResync = false;
        bool fpsMismatchOut = false;
        int64_t frameDiffOut = 0;
        SMPTETimecode prevTC = currentTC_; // capture before possible snap
        if (qfPiece_ == 0) {
            std::lock_guard<std::mutex> lk(tcMutex_);
            if (tcUpdated_) {
                lastLTCUpdateMs = std::chrono::steady_clock::now();

                fpsMismatchOut = (pendingTC_.fps != currentTC_.fps);
                frameDiffOut   = std::abs(tcToTotalFrames(pendingTC_)
                                         - tcToTotalFrames(currentTC_));
                const bool discontinuous = fpsMismatchOut || (frameDiffOut > 4);

                tcUpdated_ = false; // always consume

                if (discontinuous) {
                    currentTC_ = pendingTC_; // snap
                    // Round down to an even frame so advanceByTwo() never skips
                    // frame 00 at seconds boundaries.  The LTC source delivers
                    // odd frames (3-frame stride from an odd start) and without
                    // alignment the QF counter crosses the second on frame :29
                    // (advancing to :01) rather than :28 (advancing to :00).
                    currentTC_.frames &= ~static_cast<uint8_t>(1u);
                    doResync   = true;
                }
                // Otherwise: internal counter (advanceByTwo) keeps running.
            }
        }

        // Perform resync work OUTSIDE tcMutex_ so the LTC decode thread is
        // never blocked by MIDI I/O or Logger while calling setTimecode().
        if (doResync) {
            Logger::info(QString("MTCGenerator: resync "
                                 "(was %1:%2:%3:%4 → now %5:%6:%7:%8, "
                                 "jump=%9 frames, fpsChange=%10)")
                             .arg(prevTC.hours,       2, 10, QChar('0'))
                             .arg(prevTC.minutes,     2, 10, QChar('0'))
                             .arg(prevTC.seconds,     2, 10, QChar('0'))
                             .arg(prevTC.frames,      2, 10, QChar('0'))
                             .arg(currentTC_.hours,   2, 10, QChar('0'))
                             .arg(currentTC_.minutes, 2, 10, QChar('0'))
                             .arg(currentTC_.seconds, 2, 10, QChar('0'))
                             .arg(currentTC_.frames,  2, 10, QChar('0'))
                             .arg(frameDiffOut).arg(fpsMismatchOut));
            sendFullFrame(currentTC_);
            lastFullFrameMs = std::chrono::steady_clock::now();
            // qfPiece_ is already 0; QF stream continues from snapped TC.
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
        HighResTimer::sleepUntilNs(sleepUntil);
    }

    // Busy-wait
    while (HighResTimer::nowNs() < targetNs) {
        // spin
    }
}

} // namespace StudioLog
