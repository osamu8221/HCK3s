#ifndef DOWNBEATDETECTOR_H
#define DOWNBEATDETECTOR_H

#include <Arduino.h>
#include "IMUReader.h"

// ダウンビートとして認識するジャイロ合成角速度の閾値 [deg/s]
#define DOWNBEAT_GYRO_THRESHOLD 100.0f
// 小節単位での誤検出防止 [ms]
#define DOWNBEAT_DEBOUNCE_MS    1000

class DownbeatDetector {
public:
    // ジャイロ合成角速度が閾値を超えた場合に true を返す。
    bool detectDownbeat(IMUData data);

private:
    unsigned long lastDownbeatTime = 0;
};

#endif
