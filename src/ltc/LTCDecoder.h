#pragma once
#include "midi/MTCTypes.h"
#include "ndi/AudioRingBuffer.h"
#include <QObject>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

// Forward-declare libltc frame type so ltc.h stays out of this header
// (ltc.h is included in LTCDecoder.cpp only, away from MOC's reach)
struct LTCSMPTEFrame;

namespace StudioLog {

class FrameValidator;
class FrameRateDetector;

/// Consumes audio samples from the ring buffer on a HIGH-priority thread,
/// feeds them to libltc 256 samples at a time, and validates decoded frames.
///
/// On lock acquisition, emits frameDecoded() and lockChanged(true).
/// On dropout (no valid frame for the freewheel window), emits lockChanged(false).
class LTCDecoder : public QObject
{
    Q_OBJECT

public:
    explicit LTCDecoder(QObject* parent = nullptr);
    ~LTCDecoder() override;

    /// Start consuming from @p buffer on a background thread.
    void start(AudioRingBuffer<8192>& buffer);

    /// Stop the decode thread.
    void stop();

    bool isLocked() const { return locked_.load(std::memory_order_relaxed); }

    /// Thread-safe snapshot of the most-recently decoded timecode.
    SMPTETimecode latestFrame() const;

signals:
    /// Emitted (queued to main thread) with each validated frame.
    void frameDecoded(StudioLog::SMPTETimecode tc);

    /// Emitted on lock acquisition (true) and dropout (false).
    void lockChanged(bool locked);

private:
    void threadFunc();
    void processDecodedFrame(const LTCSMPTEFrame& raw, long pos);
    void onLockAcquired(const SMPTETimecode& tc);
    void onLockLost();

    // libltc handle — stored as void* to avoid name clash with this class name;
    // cast to ::LTCDecoder* at call sites in the .cpp
    void* ltcDec_  = nullptr;
    int         apvPerFrame_ = 1601; ///< Audio samples per LTC frame (auto-updated)

    AudioRingBuffer<8192>* ringBuffer_ = nullptr;

    std::thread        thread_;
    std::atomic<bool>  running_{false};
    std::atomic<bool>  stopRequested_{false};
    std::atomic<bool>  locked_{false};

    mutable std::mutex        frameMutex_;
    SMPTETimecode             latestFrame_;

    std::unique_ptr<FrameValidator>    validator_;
    std::unique_ptr<FrameRateDetector> rateDetector_;

    static constexpr int CHUNK_SIZE = 256; ///< Samples fed to libltc per call
};

} // namespace StudioLog
