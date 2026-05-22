#include <QtTest/QtTest>
#include "midi/MTCTypes.h"

using namespace StudioLog;

class TestMTCTiming : public QObject
{
    Q_OBJECT

private slots:
    void testQFIntervalNs_30fps();
    void testQFIntervalNs_25fps();
    void testQFIntervalNs_24fps();
    void testQFIntervalNs_2997fps();
    void testFpsCode();
    void testAdvanceByTwo_minuteBoundary();
    void testAdvanceByTwo_hourBoundary();
    void testAdvanceByTwo_dropFrame_minuteSkip();
    void testAdvanceByTwo_dropFrame_tenthMinuteNoSkip();
    void testEightQFCoversTwoFrames();
};

void TestMTCTiming::testQFIntervalNs_30fps()
{
    // 1e9 / 30 / 4 = 8_333_333 ns
    QCOMPARE(qfIntervalNs(FPS::FPS_30), int64_t(8'333'333));
}

void TestMTCTiming::testQFIntervalNs_25fps()
{
    // 1e9 / 25 / 4 = 10_000_000 ns
    QCOMPARE(qfIntervalNs(FPS::FPS_25), int64_t(10'000'000));
}

void TestMTCTiming::testQFIntervalNs_24fps()
{
    // 1e9 / 24 / 4 = 10_416_667 ns
    QCOMPARE(qfIntervalNs(FPS::FPS_24), int64_t(10'416'667));
}

void TestMTCTiming::testQFIntervalNs_2997fps()
{
    // 1e9 / 29.97 / 4 ≈ 8_341_583 ns
    QCOMPARE(qfIntervalNs(FPS::FPS_2997DF), int64_t(8'341'583));
}

void TestMTCTiming::testFpsCode()
{
    QCOMPARE(fpsCode(FPS::FPS_24),     uint8_t(0));
    QCOMPARE(fpsCode(FPS::FPS_23976),  uint8_t(0));
    QCOMPARE(fpsCode(FPS::FPS_25),     uint8_t(1));
    QCOMPARE(fpsCode(FPS::FPS_2997DF), uint8_t(2));
    QCOMPARE(fpsCode(FPS::FPS_30),     uint8_t(3));
}

void TestMTCTiming::testAdvanceByTwo_minuteBoundary()
{
    SMPTETimecode tc;
    tc.fps = FPS::FPS_30;
    tc.seconds = 59;
    tc.frames  = 28;  // advance by 2 → frame 30 → wrap to 0, seconds++
    tc.minutes = 5;

    tc.advanceByTwo();

    QCOMPARE(tc.frames,  uint8_t(0));
    QCOMPARE(tc.seconds, uint8_t(0));
    QCOMPARE(tc.minutes, uint8_t(6));
    QCOMPARE(tc.hours,   uint8_t(0));
}

void TestMTCTiming::testAdvanceByTwo_hourBoundary()
{
    SMPTETimecode tc;
    tc.fps     = FPS::FPS_30;
    tc.hours   = 23;
    tc.minutes = 59;
    tc.seconds = 59;
    tc.frames  = 28;

    tc.advanceByTwo();

    QCOMPARE(tc.frames,  uint8_t(0));
    QCOMPARE(tc.seconds, uint8_t(0));
    QCOMPARE(tc.minutes, uint8_t(0));
    QCOMPARE(tc.hours,   uint8_t(0)); // wraps midnight
}

void TestMTCTiming::testAdvanceByTwo_dropFrame_minuteSkip()
{
    // 29.97DF: at the start of minute 1 (not multiple of 10), skip frames 0 & 1
    SMPTETimecode tc;
    tc.fps       = FPS::FPS_2997DF;
    tc.dropFrame = true;
    tc.hours     = 0;
    tc.minutes   = 0;
    tc.seconds   = 59;
    tc.frames    = 28; // advance → frames=0, seconds=0, minutes=1

    tc.advanceByTwo();

    QCOMPARE(tc.minutes, uint8_t(1));
    QCOMPARE(tc.seconds, uint8_t(0));
    QCOMPARE(tc.frames,  uint8_t(2)); // frames 0 and 1 were skipped
}

void TestMTCTiming::testAdvanceByTwo_dropFrame_tenthMinuteNoSkip()
{
    // At minute 10 (multiple of 10), no skip
    SMPTETimecode tc;
    tc.fps       = FPS::FPS_2997DF;
    tc.dropFrame = true;
    tc.minutes   = 9;
    tc.seconds   = 59;
    tc.frames    = 28;

    tc.advanceByTwo();

    QCOMPARE(tc.minutes, uint8_t(10));
    QCOMPARE(tc.seconds, uint8_t(0));
    QCOMPARE(tc.frames,  uint8_t(0)); // no skip at minute × 10
}

void TestMTCTiming::testEightQFCoversTwoFrames()
{
    // Eight consecutive QF messages (pieces 0–7) must represent the same
    // timecode snapshot; caller advances TC after piece 7.
    SMPTETimecode tc;
    tc.fps     = FPS::FPS_30;
    tc.frames  = 5;
    tc.seconds = 12;
    tc.minutes = 34;
    tc.hours   = 1;

    for (uint8_t piece = 0; piece < 8; ++piece) {
        auto qf = MTCQuarterFrame::make(piece, tc);
        QCOMPARE(qf.status, uint8_t(0xF1));
        // data high nibble must equal piece
        QCOMPARE(uint8_t(qf.data >> 4u), piece);
    }

    // After all 8 pieces, advance by 2
    tc.advanceByTwo();
    QCOMPARE(tc.frames, uint8_t(7));
}

QTEST_MAIN(TestMTCTiming)
#include "test_mtc_timing.moc"
