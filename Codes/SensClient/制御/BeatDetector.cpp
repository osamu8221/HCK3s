#include "BeatDetector.h"

bool BeatDetector::detectPeak(float motion) {
    unsigned long now = millis();
    if (motion > BEAT_THRESHOLD && (now - lastPeakTime) > BEAT_DEBOUNCE_MS) {
        if (lastPeakTime > 0) {
            lastInterval = now - lastPeakTime;
        }
        lastPeakTime = now;
        return true;
    }
    return false;
}
