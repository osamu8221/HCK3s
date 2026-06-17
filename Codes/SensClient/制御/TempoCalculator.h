#ifndef TEMPOCALCULATOR_H
#define TEMPOCALCULATOR_H

#include <Arduino.h>

// 平均化するBPMサンプル数 (= 1小節分の拍数に相当)
#define BPM_BUF_SIZE     4
// Slow / Normal / Fast の境界 [BPM]
#define BPM_SLOW_THRESH  80
#define BPM_FAST_THRESH 120

class TempoCalculator {
public:
    // 拍間隔 [ms] から瞬時BPMを算出する。
    float calcBPM(unsigned long intervalMs);
    // 瞬時BPMをバッファに追加する。
    void addBPM(float bpm);
    // バッファ内の平均BPMを返す。
    float averageBPM();
    // BPMを3段階 (1=Slow / 2=Normal / 3=Fast) に変換する。
    int convertLevel(float bpm);

private:
    float intervalBuf[BPM_BUF_SIZE] = {};
    int bufIdx = 0;
    int count = 0;
};

#endif
