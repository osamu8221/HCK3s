#include "Syncfunc.h"
#include "config.h"

/*
 * SyncMain 通信遅延・成功率テスト用スケッチ
 */

SyncClass sync;
WiFiUDP ackUdp;

const unsigned long ACK_PAIR_TIMEOUT_US = 100000UL;
const unsigned long ACK_DUPLICATE_WINDOW_US = 100000UL;

char ackCommand = 0;
unsigned long ackPairStartedAt = 0;
unsigned long sensArrival = 0;
unsigned long instArrival = 0;
bool ackPairActive = false;

char ignoredCommand = 0;
unsigned long ignoreDuplicatesUntil = 0;

char reportCommand = 0;
unsigned long reportSensArrival = 0;
unsigned long reportInstArrival = 0;
bool reportPending = false;

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
        // UDPペイロードの解析やシリアル出力より先に到着時刻を確定する。
        unsigned long arrivedAt = micros();
        char ack[4] = {0};
        int len = ackUdp.read(ack, 3);
        if (len == 3 && ack[0] == 'A' &&
            (ack[1] == 's' || ack[1] == 'i')) {
            char command = ack[2];

            // 完了済み計測のUDP再送分は、新しい組として扱わない。
            bool duplicateOfCompletedPair = command == ignoredCommand &&
                (long)(ignoreDuplicatesUntil - arrivedAt) > 0;
            if (duplicateOfCompletedPair) {
                packetSize = ackUdp.parsePacket();
                continue;
            }

            // 片方のACKが欠落した組は期限切れにし、次回へ持ち越さない。
            if (ackPairActive &&
                (unsigned long)(arrivedAt - ackPairStartedAt) > ACK_PAIR_TIMEOUT_US) {
                ackPairActive = false;
            }

            if (!ackPairActive || command != ackCommand) {
                ackPairActive = true;
                ackCommand = command;
                ackPairStartedAt = arrivedAt;
                sensArrival = 0;
                instArrival = 0;
            }

            // 再送ACKでは最初の到着時刻を上書きしない。
            if (ack[1] == 's' && sensArrival == 0) sensArrival = arrivedAt;
            if (ack[1] == 'i' && instArrival == 0) instArrival = arrivedAt;

            if (sensArrival > 0 && instArrival > 0) {
                reportCommand = command;
                reportSensArrival = sensArrival;
                reportInstArrival = instArrival;
                reportPending = true;

                ackPairActive = false;
                ignoredCommand = command;
                ignoreDuplicatesUntil = arrivedAt + ACK_DUPLICATE_WINDOW_US;
            }
        }
        packetSize = ackUdp.parsePacket();
    }

    // ACKが追加で届かない場合も、欠落した組を自力で期限切れにする。
    if (ackPairActive &&
        (unsigned long)(micros() - ackPairStartedAt) > ACK_PAIR_TIMEOUT_US) {
        ackPairActive = false;
        sensArrival = 0;
        instArrival = 0;
    }
}

void printAckReport() {
    if (!reportPending) return;

    unsigned long diff = reportSensArrival > reportInstArrival
        ? reportSensArrival - reportInstArrival
        : reportInstArrival - reportSensArrival;
    Serial.print("CHILD ARRIVAL GAP [");
    Serial.print(commandName(reportCommand));
    Serial.print("] sens=");
    Serial.print(reportSensArrival);
    Serial.print(" us, inst1=");
    Serial.print(reportInstArrival);
    Serial.print(" us, diff=");
    Serial.print(diff);
    Serial.println(" us");
    reportPending = false;
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
    // ACK確認を最優先し、2台分が揃うまで通常処理へ戻らない。
    receiveAcks();
    if (ackPairActive) return;
    printAckReport();

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
