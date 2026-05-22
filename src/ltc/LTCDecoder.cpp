#include "LTCDecoder.h"
#include "FrameValidator.h"
#include "FrameRateDetector.h"
#include "util/Logger.h"
#include <ltc.h>
#include <QMetaObject>
#include <array>
#include <chrono>

#ifdef _WIN32
#  include <windows.h>
#endif

namespace StudioLog {

LTCDecoder::LTCDecoder(QObject* parent)
    : QObject(parent)
    , validator_(std::make_unique<FrameValidator>())
    , rateDetector_(std::make_unique<FrameRateDetector>())
{}

LTCDecoder::~LTCDecoder()
{
    stop();
}

// ── Public API ────────────────────────────────────────────────────────────────

void LTCDecoder::start(AudioRingBuffer<8192>& buffer)
{
    stop();
    ringBuffer_ = &buffer;

    ltcDec_ = ltc_decoder_create(apvPerFrame_, /*queue_size=*/32);
    if (!ltcDec_) {
        Logger::error("LTCDecoder: ltc_decoder_create() failed");
        return;
    }

    stopRequested_.store(false, std::memory_order_relaxed);
    running_.store(true,  std::memory_order_relaxed);
    thread_ = std::thread(&LTCDecoder::threadFunc, this);
}

void LTCDecoder::stop()
{
    stopRequested_.store(true, std::memory_order_relaxed);
    if (thread_.joinable()) thread_.join();

    if (ltcDec_) {
        ltc_decoder_free(static_cast<::LTCDecoder*>(ltcDec_));
        ltcDec_ = nullptr;
    }
    running_.store(false, std::memory_order_relaxed);
}

SMPTETimecode LTCDecoder::latestFrame() const
{
    std::lock_guard<std::mutex> lk(frameMutex_);
    return latestFrame_;
}

// ── Decode thread ─────────────────────────────────────────────────────────────

void LTCDecoder::threadFunc()
{
#ifdef _WIN32
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
#endif

    Logger::info("LTCDecoder: thread started");

    validator_->reset();
    rateDetector_->reset();
    totalLtcFrames_ = 0;
    valFailCount_   = 0;
    lastDecodedTC_.reset();

    auto* dec = static_cast<::LTCDecoder*>(ltcDec_);
    std::array<float, CHUNK_SIZE> chunk{};
    ltc_off_t samplePos = 0;

    // Silence-dropout: count chunks consumed without any decoded LTC frame.
    // At 48 kHz / 256 samples per chunk ≈ 187.5 chunks/s → 375 chunks ≈ 2 s.
    int chunksWithoutDecoded = 0;
    static constexpr int SILENT_DROPOUT_CHUNKS = 375;

    while (!stopRequested_.load(std::memory_order_relaxed)) {
        std::size_t got = ringBuffer_->read(chunk.data(), CHUNK_SIZE);
        if (got == 0) {
            // Ring buffer empty — NDI receiver hasn't produced data yet.
            // Yield briefly to avoid spinning the CPU.
            std::this_thread::sleep_for(std::chrono::microseconds(500));
            continue;
        }

        ltc_decoder_write_float(dec, chunk.data(), got, samplePos);
        samplePos += static_cast<ltc_off_t>(got);

        bool anyDecoded = false;
        LTCFrameExt frameExt{};
        while (ltc_decoder_read(dec, &frameExt)) {
            if (frameExt.reverse) continue; // skip reverse-played frames

            // Diagnostic: log first decoded frame
            if (totalLtcFrames_ == 0)
                Logger::info(QString("LTCDecoder: first LTC frame decoded (vol=%1 dBFS)")
                                 .arg(static_cast<double>(frameExt.volume), 0, 'f', 1));
            ++totalLtcFrames_;

            // RAW diagnostic: log first 30 frames from libltc (before any filtering)
            // to expose duplicate and skip patterns from this LTC source.
            if (totalLtcFrames_ <= 30) {
                ::SMPTETimecode rawSmpte{};
                ltc_frame_to_time(&rawSmpte, const_cast<LTCFrame*>(&frameExt.ltc), 0);
                Logger::info(QString("LTCDecoder[raw#%1] TC=%2:%3:%4:%5 off=[%6,%7]")
                    .arg(totalLtcFrames_,  2)
                    .arg(rawSmpte.hours,   2, 10, QChar('0'))
                    .arg(rawSmpte.mins,    2, 10, QChar('0'))
                    .arg(rawSmpte.secs,    2, 10, QChar('0'))
                    .arg(rawSmpte.frame,   2, 10, QChar('0'))
                    .arg(static_cast<long long>(frameExt.off_start))
                    .arg(static_cast<long long>(frameExt.off_end)));
            }

            anyDecoded = true;

            // Update frame-rate estimate (apvPerFrame_ is informational; libltc
            // tracks speed dynamically so we don't need to recreate the decoder)
            FPS detectedFps = rateDetector_->feed(static_cast<long>(frameExt.off_end));
            apvPerFrame_    = rateDetector_->samplesPerFrame();
            (void)detectedFps; // used implicitly via detectedFPS() in processDecodedFrame

            processDecodedFrame(frameExt.ltc, static_cast<long>(frameExt.off_start));
        }

        if (anyDecoded) {
            chunksWithoutDecoded = 0;
            // Validator-based dropout: sustained run of non-consecutive decoded frames
            if (locked_.load(std::memory_order_relaxed) && validator_->isDropout()) {
                onLockLost();
                validator_->reset();
            }
        } else {
            // Silence-based dropout: no LTC frame decoded for ~2 s
            if (++chunksWithoutDecoded >= SILENT_DROPOUT_CHUNKS) {
                chunksWithoutDecoded = 0;
                if (locked_.load(std::memory_order_relaxed)) {
                    onLockLost();
                    validator_->reset();
                }
            }
        }
    }

    Logger::info("LTCDecoder: thread stopped");
}

// ── Frame processing ──────────────────────────────────────────────────────────

void LTCDecoder::processDecodedFrame(const LTCFrame& raw, long /*pos*/)
{
    // Convert bit-packed LTCFrame → human-readable libltc ::SMPTETimecode
    ::SMPTETimecode ltcSmpte{};
    ltc_frame_to_time(&ltcSmpte, const_cast<LTCFrame*>(&raw), 0);

    // FrameRateDetector maps 29.97fps to FPS_2997DF regardless of DF bit.
    // Refine using the actual dfbit from the LTC bit stream.
    FPS fps = rateDetector_->detectedFPS();
    if (fps == FPS::FPS_2997DF && raw.dfbit == 0) {
        fps = FPS::FPS_2997NDF;
    }

    // Build our StudioLog::SMPTETimecode
    SMPTETimecode tc;
    tc.hours     = ltcSmpte.hours;
    tc.minutes   = ltcSmpte.mins;
    tc.seconds   = ltcSmpte.secs;
    tc.frames    = ltcSmpte.frame;
    tc.fps       = fps;
    tc.dropFrame = (raw.dfbit != 0);

    // ── Duplicate-frame filter ────────────────────────────────────────────────
    // libltc can re-detect the same LTC frame twice when the sync word of the
    // following frame is processed (the tail of the current frame looks like a
    // valid decode at a slightly different offset).  Feeding duplicate TCs to
    // the validator causes isConsecutive() to return false (diff=0 ≠ +1) and
    // prevents lock acquisition.  Drop any frame whose TC equals the last one.
    if (lastDecodedTC_.has_value() && tc == *lastDecodedTC_) return;
    lastDecodedTC_ = tc;

    // Log first 10 decoded timecodes (after dedup) so the log view shows the
    // real sequence the validator sees.
    if (totalLtcFrames_ <= 10) {
        const char* fpsTag = (fps == FPS::FPS_2997DF)  ? "29.97DF"
                           : (fps == FPS::FPS_2997NDF) ? "29.97NDF"
                           : (fps == FPS::FPS_30)      ? "30"
                           : (fps == FPS::FPS_25)      ? "25"
                           : (fps == FPS::FPS_24)      ? "24" : "23.976";
        Logger::info(QString("LTCDecoder: TC %1:%2:%3:%4 @ %5fps")
                         .arg(tc.hours,   2, 10, QChar('0'))
                         .arg(tc.minutes, 2, 10, QChar('0'))
                         .arg(tc.seconds, 2, 10, QChar('0'))
                         .arg(tc.frames,  2, 10, QChar('0'))
                         .arg(fpsTag));
    }

    if (!validator_->validate(tc)) {
        // Log first 10 validation failures (instance counter resets on each start)
        if (++valFailCount_ <= 10)
            Logger::info(QString("LTCDecoder: validate() failed (fail #%1) — "
                                 "waiting for consecutive frames")
                             .arg(valFailCount_));
        return;
    }

    // Store latest frame (latestFrame() is called from the main thread)
    {
        std::lock_guard<std::mutex> lk(frameMutex_);
        latestFrame_ = tc;
    }

    // Forward validated frame to main thread
    QMetaObject::invokeMethod(this,
        [this, tc]{ emit frameDecoded(tc); },
        Qt::QueuedConnection);

    // Signal lock acquisition on the first validated frame after startup or dropout
    if (!locked_.load(std::memory_order_relaxed)) {
        onLockAcquired(tc);
    }
}

// ── Lock state ────────────────────────────────────────────────────────────────

void LTCDecoder::onLockAcquired(const SMPTETimecode& tc)
{
    locked_.store(true, std::memory_order_relaxed);
    Logger::info(QString("LTCDecoder: lock acquired @ %1:%2:%3:%4")
                     .arg(tc.hours,   2, 10, QChar('0'))
                     .arg(tc.minutes, 2, 10, QChar('0'))
                     .arg(tc.seconds, 2, 10, QChar('0'))
                     .arg(tc.frames,  2, 10, QChar('0')));
    QMetaObject::invokeMethod(this, [this]{ emit lockChanged(true); },
                              Qt::QueuedConnection);
}

void LTCDecoder::onLockLost()
{
    locked_.store(false, std::memory_order_relaxed);
    Logger::info("LTCDecoder: lock lost — freewheel begins");
    QMetaObject::invokeMethod(this, [this]{ emit lockChanged(false); },
                              Qt::QueuedConnection);
}

} // namespace StudioLog
