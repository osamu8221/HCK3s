#ifndef SENSORCONTROLLER_H
#define SENSORCONTROLLER_H

#include <Arduino.h>
#include "IMUReader.h"
#include "MotionFilter.h"
#include "BeatDetector.h"
#include "DownbeatDetector.h"
#include "TempoCalculator.h"
#include "Sensfunc.h"  // SensClient/ 直下のファイル (スケッチrootがinclude pathに追加される)

class SensorController {
public:
    // MPU6050を初期化する。setup()から呼ぶ。
    void begin();
    // 1ループ分の処理: IMU読み取り→拍/ダウンビート検出→送信。
    // READY受信後のメインループから毎回呼ぶ。
    void update(SensClass& sender);

private:
    IMUReader        imu;
    MotionFilter     filter;
    BeatDetector     beat;
    DownbeatDetector downbeat;
    TempoCalculator  tempo;

    bool isStarted = false;  // START送信済みフラグ
};

#endif
