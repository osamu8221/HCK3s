#include "Syncfunc.h"
#include "config.h"

/*
 * SyncMain 通信遅延・成功率テスト用スケッチ
 */

SyncClass sync;
WiFiUDP ackUdp;

char ackCommand = 0;
unsigned long sensArrival = 0;
unsigned long instArrival = 0;

const char* commandName(char command) {
    if (command == 'S') return "START";
    if (command == 'R') return "ROUND";
    if (command >= '1' && command <= '3') {
        static char levelName[] = "LEVEL:0";
        levelName[6] = command;
        return levelName;
    }
    return "UNKNOWN";
}

void receiveAcks() {
    int packetSize = ackUdp.parsePacket();
    while (packetSize > 0) {
        char ack[4] = {0};
        int len = ackUdp.read(ack, 3);
        unsigned long arrivedAt = millis();
        if (len == 3 && ack[0] == 'A') {
            char command = ack[2];
            if (command != ackCommand) {
                ackCommand = command;
                sensArrival = 0;
                instArrival = 0;
            }

            if (ack[1] == 's') sensArrival = arrivedAt;
            if (ack[1] == 'i') instArrival = arrivedAt;

            Serial.print("[ACK] ");
            Serial.print(commandName(command));
            Serial.print(" from ");
            Serial.print(ack[1] == 's' ? "sens" : "inst1");
            Serial.print(" at ");
            Serial.print(arrivedAt);
            Serial.println(" ms");

            if (sensArrival > 0 && instArrival > 0) {
                unsigned long diff = sensArrival > instArrival
                    ? sensArrival - instArrival : instArrival - sensArrival;
                Serial.print(">>> CHILD ARRIVAL GAP [");
                Serial.print(commandName(command));
                Serial.print("] sens=");
                Serial.print(sensArrival);
                Serial.print(" ms, inst1=");
                Serial.print(instArrival);
                Serial.print(" ms, diff=");
                Serial.print(diff);
                Serial.println(" ms");
                sensArrival = 0;
                instArrival = 0;
            }
        }
        packetSize = ackUdp.parsePacket();
    }
}

void setup() {
    sync.Starts();
    ackUdp.begin(ACK_PORT);
    Serial.println("================================");
    Serial.println("SyncMain: Delay & Reliability Test Mode");
    Serial.println("Waiting for Sens & Inst...");
    Serial.println("================================");
}

void loop(){
    receiveAcks();
    // 演奏中は接続監視の頻度を下げて、中継（受信）のレスポンスを上げる
    static unsigned long lastCheck = 0;
    if (sync.ready < 2 || millis() - lastCheck > 500) {
        sync.connection();
        lastCheck = millis();
    }

    if (sync.ready == 1) {
        Serial.println("[TIME] Sending READY to all clients...");
        sync.sendReady();
        sync.ready = 2;
        Serial.println(">>> Ready sent. Monitoring sensor signals...");
    } 
    
    if (sync.ready == 2) {
        // センサからの指示受信を監視
        sync.recieveFrag();
    }

    receiveAcks();
}
