#include <QtTest/QtTest>
#include "ltc/FrameRateDetector.h"
#include "ndi/AudioRingBuffer.h"

using namespace StudioLog;

class TestLTCDecoder : public QObject
{
    Q_OBJECT

private slots:
    void testRingBufferWriteRead();
    void testRingBufferFull();
    void testRingBufferReset();
    void testFrameRateDetector_30fps();
    void testFrameRateDetector_25fps();
    void testFrameRateDetector_2997fps();
};

void TestLTCDecoder::testRingBufferWriteRead()
{
    AudioRingBuffer<256> buf;
    QCOMPARE(buf.readAvailable(), std::size_t(0));
    QCOMPARE(buf.writeAvailable(), std::size_t(256));

    float in[8]  = {1, 2, 3, 4, 5, 6, 7, 8};
    float out[8] = {};

    std::size_t written = buf.write(in, 8);
    QCOMPARE(written, std::size_t(8));
    QCOMPARE(buf.readAvailable(), std::size_t(8));

    std::size_t read = buf.read(out, 8);
    QCOMPARE(read, std::size_t(8));
    QCOMPARE(buf.readAvailable(), std::size_t(0));

    for (int i = 0; i < 8; ++i) {
        QCOMPARE(out[i], in[i]);
    }
}

void TestLTCDecoder::testRingBufferFull()
{
    AudioRingBuffer<8> buf;
    float in[8] = {0,1,2,3,4,5,6,7};

    std::size_t written = buf.write(in, 8);
    QCOMPARE(written, std::size_t(8));
    QCOMPARE(buf.writeAvailable(), std::size_t(0));

    // Writing to a full buffer should write 0 samples
    float extra = 99.f;
    std::size_t overflow = buf.write(&extra, 1);
    QCOMPARE(overflow, std::size_t(0));
}

void TestLTCDecoder::testRingBufferReset()
{
    AudioRingBuffer<64> buf;
    float s = 1.f;
    buf.write(&s, 1);
    QCOMPARE(buf.readAvailable(), std::size_t(1));

    buf.reset();
    QCOMPARE(buf.readAvailable(), std::size_t(0));
    QCOMPARE(buf.writeAvailable(), std::size_t(64));
}

void TestLTCDecoder::testFrameRateDetector_30fps()
{
    FrameRateDetector det;
    const int sr  = 48000;
    const int spf = sr / 30; // 1600 samples per frame

    for (int i = 1; i <= 15; ++i) {
        det.feed(static_cast<long>(i * spf), sr);
    }
    QCOMPARE(det.detectedFPS(), FPS::FPS_30);
}

void TestLTCDecoder::testFrameRateDetector_25fps()
{
    FrameRateDetector det;
    const int sr  = 48000;
    const int spf = sr / 25; // 1920

    for (int i = 1; i <= 15; ++i) {
        det.feed(static_cast<long>(i * spf), sr);
    }
    QCOMPARE(det.detectedFPS(), FPS::FPS_25);
}

void TestLTCDecoder::testFrameRateDetector_2997fps()
{
    FrameRateDetector det;
    const int sr  = 48000;
    // 29.97 ≈ 1601.6 samples/frame; alternate 1601/1602
    for (int i = 1; i <= 15; ++i) {
        long pos = static_cast<long>(i * 1601.6);
        det.feed(pos, sr);
    }
    FPS detected = det.detectedFPS();
    QVERIFY(detected == FPS::FPS_2997DF || detected == FPS::FPS_2997NDF);
}

QTEST_MAIN(TestLTCDecoder)
#include "test_ltc_decoder.moc"
