#include "Sensfunc.h"
#include "config.h"
#include <WiFiUdp.h>

/*
 * SensClient 最小遅延テスト用スケッチ
 */

SensClass sens;
WiFiUDP udp;
IPAddress syncIP;

unsigned long bootAt = 0;
bool connectionReady = false;
unsigned long lastHelloMs = 0;

unsigned int nextSeq = 1;
unsigned int pendingSeq = 0;
char pendingCommand = 0;
int pendingBpm = 0;
unsigned long sendTimeMs = 0;
unsigned long waitStartedAtMs = 0;
bool waitingForSyncAck = false;
unsigned long lastStatusMs = 0;

bool parseSyncAck(char *message, char &kind, char &command, unsigned int &seq) {
    if ((message[0] != 'K' && message[0] != 'T') ||
        message[1] != '|' || message[2] == 0 ||
        message[3] != '|') {
        return false;
    }
    kind = message[0];
    command = message[2];
    seq = (unsigned int)atoi(message + 4);
    return seq > 0;
}

void sendHello() {
    udp.beginPacket(syncIP, ACK_PORT);
    udp.print("HELLO|");
    udp.print(myname);
    udp.endPacket();
}

void sendSensorCommand(char command, unsigned int seq, int bpm) {
    char payload[20];
    if (command == 'B') {
        snprintf(payload, sizeof(payload), "C|%c|%u|%d", command, seq, bpm);
    } else {
        snprintf(payload, sizeof(payload), "C|%c|%u", command, seq);
    }
    udp.beginPacket(syncIP, ACK_PORT);
    udp.print(payload);
    udp.endPacket();
}

void sendSensorMetric(char command, unsigned int seq, unsigned long sensorToSyncMs) {
    char payload[20];
    snprintf(payload, sizeof(payload), "M|%c|%u|%lu", command, seq, sensorToSyncMs);
    udp.beginPacket(syncIP, ACK_PORT);
    udp.print(payload);
    udp.endPacket();
}

void startMeasurement(char command, int bpm) {
    if (command == 'B' && (bpm < MIN_BPM || bpm > MAX_BPM)) {
        Serial.print("無効なBPM: ");
        Serial.println(bpm);
        return;
    }
    pendingCommand = command;
    pendingBpm = bpm;
    pendingSeq = nextSeq++;
    sendTimeMs = millis();
    waitStartedAtMs = sendTimeMs;
    waitingForSyncAck = true;
    sendSensorCommand(command, pendingSeq, bpm);
    Serial.print("SENSOR_SEND ");
    Serial.print(command);
    Serial.print("#");
    Serial.print(pendingSeq);
    if (command == 'B') {
        Serial.print(" bpm=");
        Serial.print(bpm);
    }
    Serial.print(" sensor_send_ms=");
    Serial.println(sendTimeMs);
}

void receivePackets() {
    int packetSize = udp.parsePacket();
    if (!packetSize) {
        return;
    }

    unsigned long recvTimeMs = millis();
    char buf[32];
    int len = udp.read(buf, sizeof(buf) - 1);
    if (len <= 0) {
        return;
    }
    buf[len] = 0;
    while (len > 0 &&
        (buf[len - 1] == '\r' || buf[len - 1] == '\n' || buf[len - 1] == ' ')) {
        buf[--len] = 0;
    }

    if (strcmp(buf, "WELCOME") == 0) {
        sens.isRegistered = true;
        return;
    }
    if (strcmp(buf, "READY") == 0) {
        sens.isRegistered = true;
        sens.ready = 1;
        Serial.println("SensClient: READY");
        return;
    }

    char kind = 0;
    char command = 0;
    unsigned int seq = 0;
    if (parseSyncAck(buf, kind, command, seq) && waitingForSyncAck &&
        command == pendingCommand && seq == pendingSeq) {
        unsigned long sensorToSyncMs = recvTimeMs > sendTimeMs ?
            (recvTimeMs - sendTimeMs) / 2 : 0;
        sendSensorMetric(command, seq, sensorToSyncMs);
        waitingForSyncAck = false;
        Serial.print(kind == 'K' ? "SENSOR_RESULT " : "SENSOR_TIMEOUT_ACK ");
        Serial.print(command);
        Serial.print("#");
        Serial.print(seq);
        Serial.print(" sensor_send_ms=");
        Serial.print(sendTimeMs);
        Serial.print(" sensor_ack_recv_ms=");
        Serial.print(recvTimeMs);
        Serial.print(" sensor_to_sync_ms=");
        Serial.println(sensorToSyncMs);
        pendingCommand = 0;
        pendingBpm = 0;
        pendingSeq = 0;
    }
}

void printStatus() {
    unsigned long now = millis();
    if (now - lastStatusMs < 2000UL) {
        return;
    }
    lastStatusMs = now;
    Serial.print("SENS_STATUS ready=");
    Serial.print(sens.ready);
    Serial.print(" waiting=");
    Serial.println(waitingForSyncAck ? 1 : 0);
}

void setup() {
    sens.Starts();
    syncIP.fromString(SERVER_IP);
    udp.begin(UDP_PORT);
    bootAt = millis();
    Serial.println("SensClient: Minimal Delay Test Ready");
}

void loop() {
    receivePackets();

    if (!connectionReady && millis() - bootAt >= INITIAL_CONNECT_DELAY_MS) {
        connectionReady = true;
    }
    if (connectionReady && sens.ready == 0 && millis() - lastHelloMs >= 100) {
        lastHelloMs = millis();
        sendHello();
    }

    if (sens.ready != 0 && Serial.available() > 0 && !waitingForSyncAck) {
        char cmd = Serial.read();
        if (cmd == 's') startMeasurement('S', 0);
        else if (cmd == 'x') startMeasurement('X', 0);
        else if (cmd == '1' || cmd == '2' || cmd == '3') startMeasurement(cmd, 0);
        else if (cmd == 'b') {
            String bpmText = Serial.readStringUntil('\n');
            bpmText.trim();
            startMeasurement('B', bpmText.toInt());
        }
    }

    if (waitingForSyncAck &&
        (unsigned long)(millis() - waitStartedAtMs) > 1000UL) {
        Serial.print("SENSOR_TIMEOUT ");
        Serial.print(pendingCommand);
        Serial.print("#");
        Serial.println(pendingSeq);
        waitingForSyncAck = false;
        pendingCommand = 0;
        pendingBpm = 0;
        pendingSeq = 0;
    }

    printStatus();
}
