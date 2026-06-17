#include "SensorController.h"

void SensorController::begin() {
    if (!imu.begin()) {
        Serial.println("[SENSOR] MPU6050 init FAILED (I2C error)");
    } else {
        Serial.println("[SENSOR] MPU6050 ready");
    }
}

void SensorController::update(SensClass& sender) {
    // 1. IMUデータ取得
    IMUData raw = imu.readIMU();

    // 2. 前処理: 重力除去 → 合成加速度算出 → 平滑化
    IMUData accel  = filter.removeGravity(raw);
    float motion   = filter.calcMotion(accel);
    float smoothed = filter.smooth(motion);

    // 3. 拍検出 → 瞬時BPMをバッファへ蓄積
    if (beat.detectPeak(smoothed) && beat.lastInterval > 0) {
        float instantBPM = tempo.calcBPM(beat.lastInterval);
        tempo.addBPM(instantBPM);
        // デバッグ: Serial.print("[BEAT] BPM="); Serial.println(instantBPM);
    }

    // 4. ダウンビート検出
    if (downbeat.detectDownbeat(raw)) {
        if (!isStarted) {
            // 初回ダウンビート → 演奏開始
            sender.sendStart();
            isStarted = true;
            Serial.println("[SENSOR] Downbeat → START sent");
        } else {
            // 2回目以降 → 直近1小節のBPM平均でテンポレベルを送信
            float avgBPM = tempo.averageBPM();
            if (avgBPM > 0) {
                int lvl = tempo.convertLevel(avgBPM);
                sender.sendSpeed(lvl);
                Serial.print("[SENSOR] Downbeat → LEVEL ");
                Serial.print(lvl);
                Serial.print(" (avg BPM=");
                Serial.print(avgBPM);
                Serial.println(")");
            }
        }
    }
}
