#pragma once
#include "AudioRingBuffer.h"
#include <QObject>
#include <QString>
#include <atomic>
#include <thread>

namespace StudioLog {

/// Receives audio from an NDI source on a TIME_CRITICAL background thread.
///
/// Audio arrives as planar float32 (typically 48 kHz).  The selected channel
/// (left=0, right=1, auto=-1) is extracted and written to the shared ring
/// buffer which LTCDecoder consumes.
///
/// LTC channel selection:
///   - Left (0) or Right (1): hard-coded index into NDI audio planes
///   - Auto (-1): compares RMS of both channels over a short window and picks
///                the louder one; re-evaluated every ~1 s
///
/// IMPORTANT: the NDI source is a Raspberry Pi LTC appliance.  Confirm which
/// channel the Pi outputs on before setting a non-Auto default in Settings.
class NDIReceiver : public QObject
{
    Q_OBJECT

public:
    explicit NDIReceiver(QObject* parent = nullptr);
    ~NDIReceiver() override;

    /// Connect to the named NDI source and start the receive thread.
    void connectToSource(const QString& sourceName, int ltcChannel = -1);

    /// Disconnect and stop the receive thread.
    void disconnect();

    /// The shared ring buffer between this thread and LTCDecoder.
    AudioRingBuffer<8192>& ringBuffer() { return ringBuffer_; }

    bool isRunning() const { return running_.load(std::memory_order_relaxed); }

signals:
    /// Emitted (queued to main thread) when connection status changes.
    void connected(const QString& sourceName);
    void disconnected();
    void sampleRateChanged(int hz);

private:
    void threadFunc();
    int  autoDetectChannel(const float* left, const float* right, int samples);

    std::thread           thread_;
    std::atomic<bool>     running_{false};
    std::atomic<bool>     stopRequested_{false};

    void*       recvInstance_ = nullptr; ///< NDIlib_recv_instance_t
    std::string sourceName_;             ///< stored for reconnect
    int         ltcChannel_   = -1;      ///< -1 = auto, 0 = left, 1 = right
    int         sampleRate_   = 48000;

    // Auto-detect state: accumulated mean-square per channel across frames
    // Re-evaluated every AUTO_DETECT_FRAMES audio frames (~1 s at 30 fps)
    static constexpr int AUTO_DETECT_FRAMES = 30;
    float autoRmsAccumL_    = 0.f;
    float autoRmsAccumR_    = 0.f;
    int   autoFrameCount_   = 0;
    int   autoDetectedCh_   = 0; ///< last stable auto-detected channel

    AudioRingBuffer<8192> ringBuffer_;
};

} // namespace StudioLog
