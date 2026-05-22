#include "LTCDecoder.h"
#include "FrameValidator.h"
#include "FrameRateDetector.h"
#include "util/Logger.h"
#include <ltc.h>
#include <QMetaObject>
#include <array>

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

void LTCDecoder::start(AudioRingBuffer<8192>& buffer)
{
    stop();
    ringBuffer_ = &buffer;

    // TODO: ltcDec_ = ltc_decoder_create(apvPerFrame_, /*queue_size=*/32)
    //       apvPerFrame_ is a best-guess initial value; FrameRateDetector updates it

    stopRequested_.store(false, std::memory_order_relaxed);
    running_.store(true, std::memory_order_relaxed);
    thread_ = std::thread(&LTCDecoder::threadFunc, this);
}

void LTCDecoder::stop()
{
    stopRequested_.store(true, std::memory_order_relaxed);
    if (thread_.joinable()) thread_.join();

    // TODO: if (ltcDec_) { ltc_decoder_free(ltcDec_); ltcDec_ = nullptr; }
    running_.store(false, std::memory_order_relaxed);
}

SMPTETimecode LTCDecoder::latestFrame() const
{
    std::lock_guard<std::mutex> lk(frameMutex_);
    return latestFrame_;
}

void LTCDecoder::threadFunc()
{
#ifdef _WIN32
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
#endif

    Logger::info("LTCDecoder: thread started");

    std::array<float, CHUNK_SIZE> chunk{};
    long samplePos = 0;

    while (!stopRequested_.load(std::memory_order_relaxed)) {
        std::size_t got = ringBuffer_->read(chunk.data(), CHUNK_SIZE);
        if (got == 0) {
            // Spin-wait a very short time rather than sleeping, to keep latency low
            // TODO: replace with a condvar/semaphore signaled by NDIReceiver
            continue;
        }

        // TODO:
        //   ltc_decoder_write_float(ltcDec_, chunk.data(), static_cast<int>(got), samplePos);
        //   samplePos += got;
        //
        //   LTCFrameExt frame;
        //   while (ltc_decoder_read(ltcDec_, &frame)) {
        //       rateDetector_->feed(frame.off_end);
        //       processDecodedFrame(frame.ltc, frame.off_start);
        //   }
    }

    Logger::info("LTCDecoder: thread stopped");
}

void LTCDecoder::processDecodedFrame(const LTCSMPTEFrame& raw, long /*pos*/)
{
    // TODO: convert LTCSMPTEFrame → SMPTETimecode
    //       pass to validator_->validate(tc)
    //       if validator returns true:
    //         store in latestFrame_ (under frameMutex_)
    //         emit frameDecoded (QueuedConnection)
    //         if not locked_, call onLockAcquired()
    //
    //       reset dropout timer on every validated frame
}

void LTCDecoder::onLockAcquired(const SMPTETimecode& tc)
{
    locked_.store(true, std::memory_order_relaxed);
    Logger::info("LTCDecoder: lock acquired");
    QMetaObject::invokeMethod(this, [this]{ emit lockChanged(true); },
                              Qt::QueuedConnection);
    (void)tc;
}

void LTCDecoder::onLockLost()
{
    locked_.store(false, std::memory_order_relaxed);
    Logger::info("LTCDecoder: lock lost — freewheel begins");
    QMetaObject::invokeMethod(this, [this]{ emit lockChanged(false); },
                              Qt::QueuedConnection);
}

} // namespace StudioLog
