#include "NDIReceiver.h"
#include "util/Logger.h"
#if __has_include(<Processing.NDI.Lib.h>)
#  include <Processing.NDI.Lib.h>
#  define HAS_NDI_SDK 1
#endif
#include <QMetaObject>
#include <cmath>

#ifdef _WIN32
#  include <windows.h>
#endif

namespace StudioLog {

NDIReceiver::NDIReceiver(QObject* parent)
    : QObject(parent)
{}

NDIReceiver::~NDIReceiver()
{
    disconnect();
}

void NDIReceiver::connectToSource(const QString& sourceName, int ltcChannel)
{
    disconnect();
    ltcChannel_   = ltcChannel;
    stopRequested_.store(false, std::memory_order_relaxed);
    running_.store(true, std::memory_order_relaxed);

    // TODO:
    //   NDIlib_recv_create_v3_t cfg{};
    //   cfg.color_format = NDIlib_recv_color_format_fastest;
    //   cfg.bandwidth    = NDIlib_recv_bandwidth_audio_only;
    //   cfg.allow_video_fields = false;
    //   cfg.p_ndi_recv_name = "StudioLog NDI MIDI Bridge";
    //   recvInstance_ = NDIlib_recv_create_v3(&cfg);
    //   Set source via NDIlib_recv_connect()

    thread_ = std::thread(&NDIReceiver::threadFunc, this);
    Logger::info("NDIReceiver: connecting to " + sourceName);
}

void NDIReceiver::disconnect()
{
    stopRequested_.store(true, std::memory_order_relaxed);
    if (thread_.joinable()) thread_.join();

    // TODO: NDIlib_recv_destroy(recvInstance_)
    recvInstance_ = nullptr;
    running_.store(false, std::memory_order_relaxed);
}

void NDIReceiver::threadFunc()
{
#ifdef _WIN32
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
#endif

    Logger::info("NDIReceiver: thread started");

    while (!stopRequested_.load(std::memory_order_relaxed)) {
        // TODO:
        //   NDIlib_audio_frame_v3_t frame{};
        //   NDIlib_frame_type_e type =
        //       NDIlib_recv_capture_v3(recvInstance_, nullptr, &frame, nullptr, 100 /*ms*/);
        //
        //   if (type != NDIlib_frame_type_audio) continue;
        //
        //   if (frame.sample_rate != sampleRate_) {
        //       sampleRate_ = frame.sample_rate;
        //       QMetaObject::invokeMethod(this, [this]{ emit sampleRateChanged(sampleRate_); },
        //                                Qt::QueuedConnection);
        //   }
        //
        //   int ch = ltcChannel_;
        //   if (ch == -1) ch = autoDetectChannel(plane0, plane1, frame.no_samples);
        //
        //   const float* plane = /* frame.p_data + ch * frame.channel_stride_in_bytes / sizeof(float) */;
        //   ringBuffer_.write(plane, static_cast<size_t>(frame.no_samples));
        //   NDIlib_recv_free_audio_v3(recvInstance_, &frame);
    }

    Logger::info("NDIReceiver: thread stopped");
}

int NDIReceiver::autoDetectChannel(const float* left, const float* right, int samples)
{
    // TODO: compute RMS of each channel, return index of louder one
    (void)left; (void)right; (void)samples;
    return 0;
}

} // namespace StudioLog
