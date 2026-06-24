#include "Instfunc.h"
#include "config.h"

InstClass inst;
unsigned long bootAt = 0;
bool connectionReady = false;

bool commandTargetsThisInst(String command, const char* eventName) {
    String event(eventName);
    if (command == event) return true;

    String prefix = event + ":";
    if (!command.startsWith(prefix)) return false;

    String targets = command.substring(prefix.length());
    int start = 0;
    while (start <= targets.length()) {
        int comma = targets.indexOf(',', start);
        int end = comma >= 0 ? comma : targets.length();
        String target = targets.substring(start, end);
        target.trim();
        if (target == myname) return true;
        if (comma < 0) break;
        start = comma + 1;
    }
    return false;
}

void handleCommand(String command) {
    if (command.length() == 0) return;

    if (commandTargetsThisInst(command, "START")) {
        inst.ready = 2;
        Serial.println("START");
    } else if (commandTargetsThisInst(command, "ROUND")) {
        inst.currentRound++;
        Serial.println("ROUND");
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
    // 3. 演奏中(ready=2)は、速度レベルとラウンド指示を監視
    else {
        handleCommand(inst.recieveCommand());
    }
}
