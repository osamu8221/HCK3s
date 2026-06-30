#include "Instfunc.h"
#include "config.h"

InstClass inst;
unsigned long bootAt = 0;
bool connectionReady = false;

void handleCommand(String command) {
    if (command.length() == 0) return;

    if (command == "START") {
        inst.ready = 2;
        Serial.println("START");
    } else if (command == "STOP") {
        inst.ready = 1;
        inst.currentLevel = DEFAULT_LEVEL;
        Serial.println("STOP");
    } else if (command.startsWith("LEVEL:") && command.length() > 6) {
        inst.currentLevel = command.substring(6).toInt();
        Serial.print("LEVEL:");
        Serial.println(inst.currentLevel);
    }
}

void setup() {
    inst.Starts();
    bootAt = millis();
    Serial.println("================================");
    Serial.println("InstClient: Production Mode");
    Serial.println("Waiting for signals from SyncMain...");
    Serial.println("================================");
}

void loop() {
    // 1. まずはTCP接続と登録、READY通知を待つ
    if (inst.ready < 1){
        if (!connectionReady && millis() - bootAt >= INITIAL_CONNECT_DELAY_MS) {
            connectionReady = true;
        }
        if (connectionReady) inst.connection();
    } 
    // 2. 準備ができたら(ready=1)、UDPでの演奏開始合図(START)を待つ
    else if (inst.ready == 1) {
        handleCommand(inst.recieveCommand());
    }
    // 3. 演奏中(ready=2)は、停止と速度レベル指示を監視
    else {
        handleCommand(inst.recieveCommand());
    }
}
