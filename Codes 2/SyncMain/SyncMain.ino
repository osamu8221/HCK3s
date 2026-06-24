#include "Syncfunc.h"
#include "config.h"

SyncClass sync;

void setup() {
    sync.Starts();
    Serial.println("================================");
    Serial.println("SyncMain: Production Mode");
    Serial.println("Waiting for Sens & Inst...");
    Serial.println("================================");
}

void loop(){
    // 演奏中は接続監視の頻度を下げて、中継（受信）のレスポンスを上げる
    static unsigned long lastCheck = 0;
    if (sync.ready < 2 || millis() - lastCheck > 500) {
        sync.connection();
        lastCheck = millis();
    }

    if (sync.ready == 1) {
        sync.sendReady();
        sync.ready = 2;
    } 
    
    if (sync.ready == 2) {
        // センサからの指示受信を監視
        sync.recieveFrag();
    }
}
