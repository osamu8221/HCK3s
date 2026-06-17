#ifndef MOTIONFILTER_H
#define MOTIONFILTER_H

#include <Arduino.h>
#include "IMUReader.h"

// 移動平均のサンプル数
#define SMOOTH_N 8

class MotionFilter {
public:
    // 指数移動平均で推定した重力成分を加速度から除去する。
    IMUData removeGravity(IMUData data);
    // 3軸合成加速度 [g] を算出する。
    float calcMotion(IMUData data);
    // 移動平均でノイズを低減する。
    float smooth(float motion);

private:
    float gravX = 0, gravY = 0, gravZ = 0;
    bool gravInit = false;
    float buf[SMOOTH_N] = {};
    int bufIdx = 0;
};

#endif
