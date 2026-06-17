#include "DownbeatDetector.h"
#include <math.h>

bool DownbeatDetector::detectDownbeat(IMUData data) {
    unsigned long now = millis();
    float gyroMag = sqrtf(data.gx * data.gx + data.gy * data.gy + data.gz * data.gz);
    if (gyroMag > DOWNBEAT_GYRO_THRESHOLD && (now - lastDownbeatTime) > DOWNBEAT_DEBOUNCE_MS) {
        lastDownbeatTime = now;
        return true;
    }
    return false;
}
