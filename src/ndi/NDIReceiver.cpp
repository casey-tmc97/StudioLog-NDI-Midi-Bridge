#include "NDIReceiver.h"
#include "util/Logger.h"
#include <Processing.NDI.Lib.h>
#include <QMetaObject>
#include <cmath>
#include <cstring>

#ifdef _WIN32
#  include <windows.h>
#endif

namespace StudioLog {

// NDIlib_initialize() is called by NDIDiscovery via the shared ensureNDIInit().
// NDIReceiver assumes the library is already initialized before connectToSource()
// is called (Application::start() starts NDIDiscovery first).

NDIReceiver::NDIReceiver(QObject* parent)
    : QObject(parent)
{}

NDIReceiver::~NDIReceiver()
{
    disconnect();
}

// ── Public API ────────────────────────────────────────────────────────────────

void NDIReceiver::connectToSource(const QString& sourceName, int ltcChannel)
{
    disconnect(); // stop any existing thread/connection

    sourceName_ = sourceName.toStdString();
    ltcChannel_ = ltcChannel;
    ringBuffer_.reset();

    // Build receiver config.  Pass the source name so NDI can resolve it even
    // if we don't have the URL — it will search the network transparently.
    QByteArray nameUtf8 = sourceName.toUtf8();

    NDIlib_recv_create_v3_t cfg{};
    cfg.source_to_connect_to.p_ndi_name  = nameUtf8.constData();
    cfg.source_to_connect_to.p_url_address = nullptr;
    cfg.color_format     = NDIlib_recv_color_format_fastest; // irrelevant for audio-only
    cfg.bandwidth        = NDIlib_recv_bandwidth_audio_only; // no video decode cost
    cfg.allow_video_fields = false;
    cfg.p_ndi_recv_name  = "StudioLog NDI MIDI Bridge";

    recvInstance_ = NDIlib_recv_create_v3(&cfg);
    if (!recvInstance_) {
        Logger::error("NDIReceiver: NDIlib_recv_create_v3() failed");
        return;
    }

    Logger::info(QString("NDIReceiver: connecting to \"%1\" (ch=%2)")
                     .arg(sourceName)
                     .arg(ltcChannel == -1 ? "auto" : QString::number(ltcChannel)));

    stopRequested_.store(false, std::memory_order_relaxed);
    running_.store(true,  std::memory_order_relaxed);
    thread_ = std::thread(&NDIReceiver::threadFunc, this);
}

void NDIReceiver::disconnect()
{
    stopRequested_.store(true, std::memory_order_relaxed);
    if (thread_.joinable()) thread_.join();

    if (recvInstance_) {
        NDIlib_recv_destroy(static_cast<NDIlib_recv_instance_t>(recvInstance_));
        recvInstance_ = nullptr;
    }

    running_.store(false, std::memory_order_relaxed);
}

// ── Receive thread ────────────────────────────────────────────────────────────

void NDIReceiver::threadFunc()
{
#ifdef _WIN32
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
#endif

    auto* recv = static_cast<NDIlib_recv_instance_t>(recvInstance_);
    Logger::info("NDIReceiver: thread started");

    // Reset auto-detect accumulators
    autoRmsAccumL_  = 0.f;
    autoRmsAccumR_  = 0.f;
    autoFrameCount_ = 0;
    autoDetectedCh_ = (ltcChannel_ >= 0) ? ltcChannel_ : 0;

    bool wasConnected = false;

    while (!stopRequested_.load(std::memory_order_relaxed)) {

        NDIlib_audio_frame_v3_t frame{};
        NDIlib_frame_type_e type = NDIlib_recv_capture_v3(
            recv,
            nullptr,  // no video
            &frame,
            nullptr,  // no metadata
            100       // ms timeout — gives the stop flag a chance to be checked
        );

        switch (type) {

        case NDIlib_frame_type_audio: {
            // ── Guard: we only handle planar float32 ─────────────────────────
            if (frame.FourCC != NDIlib_FourCC_audio_type_FLTP ||
                frame.no_channels < 1 ||
                frame.no_samples  < 1 ||
                frame.p_data      == nullptr)
            {
                NDIlib_recv_free_audio_v3(recv, &frame);
                break;
            }

            // Notify main thread of sample-rate changes
            if (frame.sample_rate != sampleRate_) {
                sampleRate_ = frame.sample_rate;
                QMetaObject::invokeMethod(this,
                    [this, sr = sampleRate_]{ emit sampleRateChanged(sr); },
                    Qt::QueuedConnection);
                Logger::info(QString("NDIReceiver: sample rate = %1 Hz").arg(sampleRate_));
            }

            if (!wasConnected) {
                wasConnected = true;
                QMetaObject::invokeMethod(this,
                    [this, n = QString::fromStdString(sourceName_)]{ emit connected(n); },
                    Qt::QueuedConnection);
                Logger::info(QString("NDIReceiver: audio flowing — %1 ch, %2 Hz, FourCC=0x%3")
                                 .arg(frame.no_channels)
                                 .arg(frame.sample_rate)
                                 .arg(frame.FourCC, 8, 16, QChar('0')));
            }

            // ── Channel pointers ─────────────────────────────────────────────
            // NDI planar float: channel N starts at p_data + N * channel_stride_in_bytes
            const auto* base = frame.p_data;
            const int   stride = frame.channel_stride_in_bytes;
            const auto* chL = reinterpret_cast<const float*>(base);
            const auto* chR = (frame.no_channels >= 2)
                              ? reinterpret_cast<const float*>(base + stride)
                              : chL; // mono source — use same channel for both

            // ── Auto channel detection ───────────────────────────────────────
            int activeCh = ltcChannel_;
            if (activeCh == -1) {
                activeCh = autoDetectChannel(chL, chR, frame.no_samples);
            }
            activeCh = std::clamp(activeCh, 0, frame.no_channels - 1);

            // ── Write selected channel to ring buffer ─────────────────────────
            const auto* samples = (activeCh == 0) ? chL : chR;
            std::size_t written = ringBuffer_.write(samples,
                                                    static_cast<std::size_t>(frame.no_samples));
            if (written < static_cast<std::size_t>(frame.no_samples)) {
                // Ring buffer full — LTC decoder isn't keeping up; not critical
                // but log occasionally (avoid flooding)
                static int overflowCount = 0;
                if (++overflowCount % 100 == 0)
                    Logger::warn("NDIReceiver: ring buffer overflow (LTC decode falling behind)");
            }

            NDIlib_recv_free_audio_v3(recv, &frame);
            break;
        }

        case NDIlib_frame_type_error:
            // Connection lost (source went offline, network drop, etc.)
            Logger::warn("NDIReceiver: connection error — source lost");
            if (wasConnected) {
                wasConnected = false;
                QMetaObject::invokeMethod(this,
                    [this]{ emit disconnected(); },
                    Qt::QueuedConnection);
            }
            // Back off briefly before the next capture attempt so we don't
            // spin-hammer on a dead connection.
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            break;

        case NDIlib_frame_type_none:
            // Timeout — no data; source may still be connecting.  Normal at startup.
            break;

        default:
            // Status change or unexpected frame type — ignore
            break;
        }
    }

    // If we disconnected cleanly while still "connected", notify the main thread
    if (wasConnected) {
        QMetaObject::invokeMethod(this, [this]{ emit disconnected(); },
                                  Qt::QueuedConnection);
    }

    Logger::info("NDIReceiver: thread stopped");
}

// ── Auto channel detection ────────────────────────────────────────────────────

int NDIReceiver::autoDetectChannel(const float* left, const float* right, int samples)
{
    // Accumulate mean-square (proportional to power) for each channel
    float msL = 0.f, msR = 0.f;
    for (int i = 0; i < samples; ++i) {
        msL += left[i]  * left[i];
        msR += right[i] * right[i];
    }
    autoRmsAccumL_ += msL / static_cast<float>(samples);
    autoRmsAccumR_ += msR / static_cast<float>(samples);
    ++autoFrameCount_;

    // Re-evaluate every AUTO_DETECT_FRAMES frames (~1 s at 30 audio frames/s)
    if (autoFrameCount_ >= AUTO_DETECT_FRAMES) {
        // 3 dB hysteresis: only switch if the other channel is meaningfully louder
        constexpr float HYSTERESIS = 2.0f; // right must be 2× louder to flip to right
        if (autoDetectedCh_ == 0) {
            if (autoRmsAccumR_ > autoRmsAccumL_ * HYSTERESIS) {
                autoDetectedCh_ = 1;
                Logger::info("NDIReceiver: auto-detect → Right channel");
            }
        } else {
            if (autoRmsAccumL_ > autoRmsAccumR_ * HYSTERESIS) {
                autoDetectedCh_ = 0;
                Logger::info("NDIReceiver: auto-detect → Left channel");
            }
        }
        // Reset accumulators for next window
        autoRmsAccumL_  = 0.f;
        autoRmsAccumR_  = 0.f;
        autoFrameCount_ = 0;
    }

    return autoDetectedCh_;
}

} // namespace StudioLog
