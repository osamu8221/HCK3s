#include <WiFiS3.h>
#include "config.h"
#include "Sensfunc.h"
#include "制御/SensorController.h"

SensClass sens;
SensorController ctrl;

void setup() {
    ctrl.begin();   // MPU6050 初期化 (WiFi接続より先に実行)
    sens.Starts();  // WiFi接続 + 親機への登録待機
}

void loop() {
    if (sens.ready == 0) {
        // 親機への登録 (WELCOME → READY)
        sens.connection();
    } else {
        // READY受信済み: センサ検出ループ
        ctrl.update(sens);
    }
}
