#include "Instfunc.h"
#include "config.h"

/*
 * InstClient 通信遅延テスト用スケッチ
 */

InstClass inst;
WiFiUDP ackUdp;
unsigned long bootAt = 0;
bool connectionReady = false;

void sendAck(char command) {
    IPAddress syncIP;
    syncIP.fromString(SERVER_IP);
    char ack[3] = {'A', 'i', command};
    ackUdp.beginPacket(syncIP, ACK_PORT);
    ackUdp.write((uint8_t*)ack, sizeof(ack));
    ackUdp.endPacket();
}

void handleCommand(String command) {
    if (command.length() == 0) return;

    // 状態更新やログ出力を始める前にACKを返す。
    char ackCommand = 0;
    if (command == "START") ackCommand = 'S';
    else if (command == "ROUND") ackCommand = 'R';
    else if (command.startsWith("LEVEL:") && command.length() > 6) {
        ackCommand = command.charAt(6);
    }
    if (ackCommand == 0) return;
    sendAck(ackCommand);

    if (command == "START") {
        inst.ready = 2;
        Serial.println("[TIME] START signal received.");
    } else if (command == "ROUND") {
        inst.currentRound++;
        Serial.println("[TIME] ROUND signal received.");
    } else if (command.startsWith("LEVEL:") && command.length() > 6) {
        inst.currentLevel = command.substring(6).toInt();
        Serial.print("[TIME] LEVEL:");
        Serial.print(inst.currentLevel);
        Serial.println(" signal received.");
    }
}

void setup() {
    inst.Starts();
    ackUdp.begin(ACK_PORT + 1);
    bootAt = millis();
    Serial.println("================================");
    Serial.println("InstClient: Delay Test Mode");
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
