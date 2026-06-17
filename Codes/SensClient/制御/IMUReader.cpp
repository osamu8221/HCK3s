#include "IMUReader.h"

#define MPU6050_ADDR  0x68
#define PWR_MGMT_1    0x6B
#define GYRO_CONFIG   0x1B
#define ACCEL_CONFIG  0x1C
#define ACCEL_XOUT_H  0x3B
// ±2g → 16384 LSB/g, ±250deg/s → 131 LSB/(deg/s)
#define ACCEL_SCALE   16384.0f
#define GYRO_SCALE    131.0f

bool IMUReader::begin() {
    Wire.begin();
    // スリープ解除
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(PWR_MGMT_1);
    Wire.write(0x00);
    if (Wire.endTransmission() != 0) return false;
    delay(10);
    // ジャイロレンジ ±250deg/s
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(GYRO_CONFIG);
    Wire.write(0x00);
    Wire.endTransmission();
    // 加速度レンジ ±2g
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(ACCEL_CONFIG);
    Wire.write(0x00);
    Wire.endTransmission();
    return true;
}

IMUData IMUReader::readIMU() {
    // 加速度(6byte) + 温度(2byte) + ジャイロ(6byte) を一括読み出し
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(ACCEL_XOUT_H);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)MPU6050_ADDR, (uint8_t)14, (uint8_t)true);

    IMUData d;
    d.ax = (float)((int16_t)(Wire.read() << 8 | Wire.read())) / ACCEL_SCALE;
    d.ay = (float)((int16_t)(Wire.read() << 8 | Wire.read())) / ACCEL_SCALE;
    d.az = (float)((int16_t)(Wire.read() << 8 | Wire.read())) / ACCEL_SCALE;
    Wire.read(); Wire.read();  // 温度スキップ
    d.gx = (float)((int16_t)(Wire.read() << 8 | Wire.read())) / GYRO_SCALE;
    d.gy = (float)((int16_t)(Wire.read() << 8 | Wire.read())) / GYRO_SCALE;
    d.gz = (float)((int16_t)(Wire.read() << 8 | Wire.read())) / GYRO_SCALE;
    return d;
}
