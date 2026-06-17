#ifndef BEATDETECTOR_H
#define BEATDETECTOR_H

#include <Arduino.h>

// 拍として認識する最小の合成加速度 [g] (重力除去後)
#define BEAT_THRESHOLD   1.0f
// 誤検出防止の最小拍間隔 [ms] (= 最大 240 BPM 相当)
#define BEAT_DEBOUNCE_MS 250

class BeatDetector {
public:
    // smoothed motion がピーク閾値を超えた場合に true を返す。
    // 検出時は lastInterval に前回拍からの経過時間 [ms] を格納する。
    bool detectPeak(float motion);

    unsigned long lastInterval = 0;  // 直前の拍間隔 [ms]

private:
    unsigned long lastPeakTime = 0;
};

#endif
