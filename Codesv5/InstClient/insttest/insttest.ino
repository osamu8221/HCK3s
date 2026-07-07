#include "Instfunc.h"
#include "config.h"

/*
 * InstClient 通信遅延テスト用スケッチ
 */

InstClass inst;
WiFiUDP ackUdp;
unsigned long bootAt = 0;
bool connectionReady = false;
unsigned long lastHelloMs = 0;
char lastCommand[16] = "";
unsigned int lastSeq = 0;
unsigned long lastStatusMs = 0;

bool parseCommandWithSeq(char *message, char *&command, unsigned int &seq) {
    char *sep = strrchr(message, '|');
    if (sep == nullptr || sep == message || sep[1] == 0) {
        return false;
    }
    *sep = 0;
    command = message;
    seq = (unsigned int)atoi(sep + 1);
    return seq > 0 && command[0] != 0;
}

void sendAck(const char *command, unsigned int seq, unsigned long instProcMs) {
    IPAddress syncIP;
    syncIP.fromString(SERVER_IP);
    ackUdp.beginPacket(syncIP, ACK_PORT);
    ackUdp.print("A|");
    ackUdp.print(myname);
    ackUdp.print("|");
    ackUdp.print(command);
    ackUdp.print("|");
    ackUdp.print(seq);
    ackUdp.print("|");
    ackUdp.print(instProcMs);
    ackUdp.endPacket();
}

void sendHello() {
    IPAddress syncIP;
    syncIP.fromString(SERVER_IP);
    ackUdp.beginPacket(syncIP, ACK_PORT);
    ackUdp.print("HELLO|");
    ackUdp.print(myname);
    ackUdp.endPacket();
}

void handleCommand(char *message) {
    if (message[0] == 0) {
        return;
    }

    if (strcmp(message, "WELCOME") == 0) {
        inst.isRegistered = true;
        Serial.println("Server registered this device (WELCOME).");
        return;
    }

    if (strcmp(message, "READY") == 0) {
        inst.isRegistered = true;
        inst.ready = 1;
        Serial.println("全員の準備が整いました (READY受信)");
        return;
    }

    char *command = nullptr;
    unsigned int seq = 0;
    unsigned long recvMs = millis();
    if (!parseCommandWithSeq(message, command, seq)) {
        return;
    }

    unsigned long ackStartMs = millis();
    unsigned long instProcMs = ackStartMs > recvMs ? ackStartMs - recvMs : 0;
    sendAck(command, seq, instProcMs);

    Serial.print("INST_RECV ");
    Serial.print(myname);
    Serial.print(" ");
    Serial.print(command);
    Serial.print("#");
    Serial.print(seq);
    Serial.print(" inst_recv_ms=");
    Serial.print(recvMs);
    Serial.print(" inst_proc_ms=");
    Serial.println(instProcMs);

    if (seq == lastSeq && strcmp(command, lastCommand) == 0) {
        return;
    }
    strncpy(lastCommand, command, sizeof(lastCommand) - 1);
    lastCommand[sizeof(lastCommand) - 1] = 0;
    lastSeq = seq;

    if (strcmp(command, "S") == 0 || strcmp(command, "START") == 0) {
        inst.ready = 2;
    } else if (strcmp(command, "X") == 0 || strcmp(command, "STOP") == 0) {
        inst.ready = 1;
        inst.currentLevel = DEFAULT_LEVEL;
        inst.currentBpm = DEFAULT_BPM;
    } else if (strcmp(command, "R") == 0 || strcmp(command, "ROUND") == 0) {
        return;
    } else if (strlen(command) == 1 && command[0] >= '1' && command[0] <= '3') {
        inst.currentLevel = command[0] - '0';
        inst.currentBpm = LEVEL_BPM[inst.currentLevel - 1];
    } else if (strncmp(command, "LEVEL:", 6) == 0 && command[6] != 0) {
        inst.currentLevel = atoi(command + 6);
        if (inst.currentLevel >= 1 && inst.currentLevel <= 3) {
            inst.currentBpm = LEVEL_BPM[inst.currentLevel - 1];
        }
    } else if (strncmp(command, "BPM:", 4) == 0 && command[4] != 0) {
        int bpm = atoi(command + 4);
        if (bpm >= MIN_BPM && bpm <= MAX_BPM) {
            inst.currentBpm = bpm;
        }
    }
}

void printStatus() {
    unsigned long now = millis();
    if (now - lastStatusMs < 2000UL) {
        return;
    }
    lastStatusMs = now;
    Serial.print("INST_STATUS ");
    Serial.print(myname);
    Serial.print(" ready=");
    Serial.print(inst.ready);
    Serial.print(" level=");
    Serial.print(inst.currentLevel);
    Serial.print(" bpm=");
    Serial.println(inst.currentBpm);
}

void setup() {
    inst.Starts();
    ackUdp.begin(ACK_PORT + 1);
    bootAt = millis();
    Serial.println("InstClient: Delay Test Ready");
}

void loop() {
    if (!connectionReady && millis() - bootAt >= INITIAL_CONNECT_DELAY_MS) {
        connectionReady = true;
    }
    if (connectionReady && inst.ready < 1) {
        if (millis() - lastHelloMs >= 100) {
            lastHelloMs = millis();
            sendHello();
        }
    }

    char message[32];
    if (inst.recieveCommandFast(message, sizeof(message))) {
        handleCommand(message);
    }

    printStatus();
}
