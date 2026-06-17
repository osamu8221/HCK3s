#include "MotionFilter.h"
#include <math.h>

// 重力推定の更新係数。小さいほど静的姿勢変化への追従が遅い。
#define GRAVITY_ALPHA 0.02f

IMUData MotionFilter::removeGravity(IMUData data) {
    if (!gravInit) {
        gravX = data.ax;
        gravY = data.ay;
        gravZ = data.az;
        gravInit = true;
    } else {
        // 低速姿勢変化のみ追従する指数移動平均 (= 重力推定)
        gravX = (1.0f - GRAVITY_ALPHA) * gravX + GRAVITY_ALPHA * data.ax;
        gravY = (1.0f - GRAVITY_ALPHA) * gravY + GRAVITY_ALPHA * data.ay;
        gravZ = (1.0f - GRAVITY_ALPHA) * gravZ + GRAVITY_ALPHA * data.az;
    }
    data.ax -= gravX;
    data.ay -= gravY;
    data.az -= gravZ;
    return data;
}

float MotionFilter::calcMotion(IMUData data) {
    return sqrtf(data.ax * data.ax + data.ay * data.ay + data.az * data.az);
}

float MotionFilter::smooth(float motion) {
    buf[bufIdx] = motion;
    bufIdx = (bufIdx + 1) % SMOOTH_N;
    float sum = 0;
    for (int i = 0; i < SMOOTH_N; i++) sum += buf[i];
    return sum / SMOOTH_N;
}
