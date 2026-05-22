#include <QtTest/QtTest>
#include "ltc/FrameValidator.h"

using namespace StudioLog;

class TestFrameValidator : public QObject
{
    Q_OBJECT

private slots:
    void testLockRequiresTwoConsecutiveFrames();
    void testDropoutDetection();
    void testResetClearsLock();
    void testMTCTypesAdvanceByTwo_30fps();
    void testMTCTypesAdvanceByTwo_rollover();
    void testMTCFullFrame_encoding();
    void testMTCQuarterFrame_piece0();
    void testMTCQuarterFrame_piece7_fps();
};

static SMPTETimecode makeTC(uint8_t h, uint8_t m, uint8_t s, uint8_t f,
                             FPS fps = FPS::FPS_30)
{
    SMPTETimecode tc;
    tc.hours = h; tc.minutes = m; tc.seconds = s; tc.frames = f;
    tc.fps = fps;
    return tc;
}

void TestFrameValidator::testLockRequiresTwoConsecutiveFrames()
{
    FrameValidator v;
    v.setConfirmationWindow(2);

    // First frame — not locked yet
    bool r1 = v.validate(makeTC(0,0,0,0));
    QVERIFY(!r1);
    QVERIFY(!v.isLocked());

    // TODO: second consecutive frame → should lock
    // (requires isConsecutive() to be implemented)
    // bool r2 = v.validate(makeTC(0,0,0,1));
    // QVERIFY(r2);
    // QVERIFY(v.isLocked());
}

void TestFrameValidator::testDropoutDetection()
{
    FrameValidator v;
    v.setDropoutThreshold(3);
    v.validate(makeTC(0,0,0,0));

    // After threshold frames without a valid call, dropout should be true
    // (framesSinceValid_ is incremented inside validate(); we need to pump)
    for (int i = 0; i < 3; ++i) v.validate(makeTC(1,0,0,0)); // non-consecutive → invalid
    QVERIFY(v.isDropout());
}

void TestFrameValidator::testResetClearsLock()
{
    FrameValidator v;
    // Force locked state (manually for now, until isConsecutive() implemented)
    v.reset();
    QVERIFY(!v.isLocked());
}

void TestFrameValidator::testMTCTypesAdvanceByTwo_30fps()
{
    SMPTETimecode tc = makeTC(0, 0, 0, 0, FPS::FPS_30);
    tc.advanceByTwo();
    QCOMPARE(tc.frames, uint8_t(2));
    QCOMPARE(tc.seconds, uint8_t(0));
}

void TestFrameValidator::testMTCTypesAdvanceByTwo_rollover()
{
    SMPTETimecode tc = makeTC(0, 0, 0, 28, FPS::FPS_30);
    tc.advanceByTwo();
    QCOMPARE(tc.frames,  uint8_t(0));
    QCOMPARE(tc.seconds, uint8_t(1));
    QCOMPARE(tc.minutes, uint8_t(0));
}

void TestFrameValidator::testMTCFullFrame_encoding()
{
    // hh = (fps_code << 5) | hours
    // 30fps → fps_code=3, hours=1 → 0b01100001 = 0x61
    SMPTETimecode tc = makeTC(1, 2, 3, 4, FPS::FPS_30);
    MTCFullFrame ff(tc);

    QCOMPARE(ff.data[0], uint8_t(0xF0));
    QCOMPARE(ff.data[1], uint8_t(0x7F));
    QCOMPARE(ff.data[2], uint8_t(0x7F));
    QCOMPARE(ff.data[3], uint8_t(0x01));
    QCOMPARE(ff.data[4], uint8_t(0x01));
    QCOMPARE(ff.data[5], uint8_t((3u << 5u) | 1u)); // fps_code=3, hours=1
    QCOMPARE(ff.data[6], uint8_t(2));
    QCOMPARE(ff.data[7], uint8_t(3));
    QCOMPARE(ff.data[8], uint8_t(4));
    QCOMPARE(ff.data[9], uint8_t(0xF7));
}

void TestFrameValidator::testMTCQuarterFrame_piece0()
{
    SMPTETimecode tc = makeTC(0, 0, 0, 13, FPS::FPS_30);
    auto qf = MTCQuarterFrame::make(0, tc);
    // piece 0: frame LSN = 13 & 0x0F = 0x0D
    // data byte = (0 << 4) | 0x0D = 0x0D
    QCOMPARE(qf.status, uint8_t(0xF1));
    QCOMPARE(qf.data,   uint8_t(0x0D));
}

void TestFrameValidator::testMTCQuarterFrame_piece7_fps()
{
    // 25fps → fps_code=1
    // hours=0, hours_MSN=0
    // piece7 data nibble = (0 & 0x01) | (1 << 1) = 0x02
    // data byte = (7 << 4) | 0x02 = 0x72
    SMPTETimecode tc = makeTC(0, 0, 0, 0, FPS::FPS_25);
    auto qf = MTCQuarterFrame::make(7, tc);
    QCOMPARE(qf.status, uint8_t(0xF1));
    QCOMPARE(qf.data,   uint8_t(0x72));
}

QTEST_MAIN(TestFrameValidator)
#include "test_frame_validator.moc"
