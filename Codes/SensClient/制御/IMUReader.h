#ifndef IMUREADER_H
#define IMUREADER_H

#include <Arduino.h>
#include <Wire.h>

struct IMUData {
    float ax, ay, az;  // [g]
    float gx, gy, gz;  // [deg/s]
};

class IMUReader {
public:
    // I2C初期化とMPU6050のウェイクアップ。成功時true。
    bool begin();
    // 3軸加速度・ジャイロを取得して返す。
    IMUData readIMU();
};

#endif
