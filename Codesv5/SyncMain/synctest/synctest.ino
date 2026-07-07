#include "Syncfunc.h"
#include "config.h"

/*
 * SyncMain 遅延テスト用スケッチ
 * STARTは実コード同様にinst1へunicast、LEVEL/STOPはbroadcastで測定する。
 */

SyncClass sync;
WiFiUDP ackUdp;

const unsigned long MEASUREMENT_TIMEOUT_US = 1000000UL;
const unsigned long DUPLICATE_WINDOW_MS = 100UL;
const unsigned long REPORT_WAIT_TIMEOUT_MS = 1000UL;
const int MAX_INST_ACKS = 4;

char activeCommand = 0;
unsigned int activeSeq = 0;
char activeInstCommand[16] = "";
char activeRoute[10] = "";
unsigned long syncRecvMs = 0;
unsigned long syncSendAtMs = 0;
unsigned long syncProcessMs = 0;
unsigned long measurementStartedAtMs = 0;
unsigned long sensorToSyncMs = 0;
bool measurementActive = false;
bool sensorMetricReceived = false;

struct InstAckRecord {
    char name[8];
    unsigned long arrivedAtMs;
};

InstAckRecord instAckRecords[MAX_INST_ACKS];
int instAckCount = 0;

char ignoredInstCommand[16] = "";
unsigned int ignoredSeq = 0;
unsigned long ignoreDuplicatesUntilMs = 0;

char reportCommand = 0;
unsigned int reportSeq = 0;
char reportInstCommand[16] = "";
char reportRoute[10] = "";
unsigned long reportSyncRecvMs = 0;
unsigned long reportSyncSendMs = 0;
unsigned long reportSensorToSyncMs = 0;
unsigned long reportInstAckRttMs = 0;
unsigned long reportSyncToInstMs = 0;
unsigned long reportInstProcMs = 0;
unsigned long reportTotalEstMs = 0;
unsigned long reportSyncProcessMs = 0;
unsigned long reportFirstInstAckMs = 0;
unsigned long reportInstArrivalDeltaMs = 0;
int reportInstAckCount = 0;
bool resultReportPending = false;
bool resultReportHasSensorMetric = false;
bool resultReportHasInstProc = false;
unsigned long resultReportCreatedAtMs = 0;
unsigned long lastStatusMs = 0;

bool parseSensorCommand(char *message, char &command, unsigned int &seq, int &bpm) {
    if (message[0] != 'C' || message[1] != '|' || message[2] == 0 ||
        message[3] != '|') {
        return false;
    }
    command = message[2];
    bpm = 0;

    char *seqStart = message + 4;
    char *sep = strchr(seqStart, '|');
    if (sep != nullptr) {
        *sep = 0;
        if (command == 'B') {
            bpm = atoi(sep + 1);
        }
    }

    seq = (unsigned int)atoi(seqStart);
    if (command == 'B' && (bpm < MIN_BPM || bpm > MAX_BPM)) {
        return false;
    }
    return seq > 0;
}

bool parseSensorMetric(char *message, char &command, unsigned int &seq, unsigned long &metricMs) {
    if (message[0] != 'M' || message[1] != '|' || message[2] == 0 ||
        message[3] != '|') {
        return false;
    }

    char *seqStart = message + 4;
    char *sep = strchr(seqStart, '|');
    if (sep == nullptr || sep == seqStart || sep[1] == 0) {
        return false;
    }

    *sep = 0;
    command = message[2];
    seq = (unsigned int)atoi(seqStart);
    metricMs = (unsigned long)atol(sep + 1);
    return seq > 0;
}

bool parseInstAck(char *message, char *name, size_t nameSize,
    char *command, size_t commandSize, unsigned int &seq,
    unsigned long &instProcMs, bool &hasInstProc) {
    const char prefix[] = "A|";
    size_t prefixLen = strlen(prefix);
    if (strncmp(message, prefix, prefixLen) != 0 || message[prefixLen] == 0) {
        return false;
    }

    char *nameStart = message + prefixLen;
    char *nameSep = strchr(nameStart, '|');
    if (nameSep == nullptr || nameSep == nameStart || nameSep[1] == 0) {
        return false;
    }
    *nameSep = 0;
    strncpy(name, nameStart, nameSize - 1);
    name[nameSize - 1] = 0;

    char *commandStart = nameSep + 1;
    char *commandSep = strchr(commandStart, '|');
    if (commandSep == nullptr || commandSep == commandStart || commandSep[1] == 0) {
        return false;
    }
    *commandSep = 0;
    strncpy(command, commandStart, commandSize - 1);
    command[commandSize - 1] = 0;

    char *seqStart = commandSep + 1;
    char *procSep = strchr(seqStart, '|');
    if (procSep != nullptr) {
        if (procSep == seqStart || procSep[1] == 0) {
            return false;
        }
        *procSep = 0;
        instProcMs = (unsigned long)atol(procSep + 1);
        hasInstProc = true;
    } else {
        instProcMs = 0;
        hasInstProc = false;
    }

    seq = (unsigned int)atoi(seqStart);
    return seq > 0;
}

bool parseHello(char *message, char *&childName) {
    const char prefix[] = "HELLO|";
    size_t prefixLen = strlen(prefix);
    if (strncmp(message, prefix, prefixLen) != 0 || message[prefixLen] == 0) {
        return false;
    }
    childName = message + prefixLen;
    return true;
}

void resetActiveMeasurement() {
    activeCommand = 0;
    activeSeq = 0;
    activeInstCommand[0] = 0;
    activeRoute[0] = 0;
    syncRecvMs = 0;
    syncSendAtMs = 0;
    syncProcessMs = 0;
    measurementStartedAtMs = 0;
    sensorToSyncMs = 0;
    measurementActive = false;
    sensorMetricReceived = false;
}

void startMeasurement(char command, unsigned int seq, const char *instCommand,
    const char *route, unsigned long receivedAtMs) {
    activeCommand = command;
    activeSeq = seq;
    strncpy(activeInstCommand, instCommand, sizeof(activeInstCommand) - 1);
    activeInstCommand[sizeof(activeInstCommand) - 1] = 0;
    strncpy(activeRoute, route, sizeof(activeRoute) - 1);
    activeRoute[sizeof(activeRoute) - 1] = 0;
    syncRecvMs = receivedAtMs;
    syncSendAtMs = 0;
    syncProcessMs = 0;
    measurementStartedAtMs = receivedAtMs;
    sensorToSyncMs = 0;
    measurementActive = true;
    sensorMetricReceived = false;
    instAckCount = 0;
}

void sendSensorResult(char prefix, char command, unsigned int seq) {
    char ack[16];
    snprintf(ack, sizeof(ack), "%c|%c|%u", prefix, command, seq);
    sync.sendUdpToSensFast(ack);
}

void updateReportSensorMetric(char command, unsigned int seq, unsigned long metricMs) {
    if (measurementActive && command == activeCommand && seq == activeSeq) {
        sensorToSyncMs = metricMs;
        sensorMetricReceived = true;
    }
    if (resultReportPending && command == reportCommand && seq == reportSeq) {
        reportSensorToSyncMs = metricMs;
        resultReportHasSensorMetric = true;
        reportTotalEstMs = reportSensorToSyncMs + reportSyncProcessMs + reportSyncToInstMs;
    }
}

void addInstAck(const char *name, unsigned long arrivedAtMs) {
    for (int i = 0; i < instAckCount; i++) {
        if (strcmp(instAckRecords[i].name, name) == 0) {
            return;
        }
    }
    if (instAckCount >= MAX_INST_ACKS) {
        return;
    }
    strncpy(instAckRecords[instAckCount].name, name,
        sizeof(instAckRecords[instAckCount].name) - 1);
    instAckRecords[instAckCount].name[sizeof(instAckRecords[instAckCount].name) - 1] = 0;
    instAckRecords[instAckCount].arrivedAtMs = arrivedAtMs;
    instAckCount++;
}

unsigned long calculateInstArrivalDeltaMs() {
    if (instAckCount < 2) {
        return 0;
    }

    unsigned long minArrived = instAckRecords[0].arrivedAtMs;
    unsigned long maxArrived = instAckRecords[0].arrivedAtMs;
    for (int i = 1; i < instAckCount; i++) {
        if (instAckRecords[i].arrivedAtMs < minArrived) {
            minArrived = instAckRecords[i].arrivedAtMs;
        }
        if (instAckRecords[i].arrivedAtMs > maxArrived) {
            maxArrived = instAckRecords[i].arrivedAtMs;
        }
    }
    return maxArrived - minArrived;
}

void finalizeMeasurement(unsigned long ackArrivedAtMs,
    unsigned long instProcMs, bool hasInstProc) {
    reportCommand = activeCommand;
    reportSeq = activeSeq;
    strncpy(reportInstCommand, activeInstCommand, sizeof(reportInstCommand) - 1);
    reportInstCommand[sizeof(reportInstCommand) - 1] = 0;
    strncpy(reportRoute, activeRoute, sizeof(reportRoute) - 1);
    reportRoute[sizeof(reportRoute) - 1] = 0;
    reportSyncRecvMs = syncRecvMs;
    reportSyncSendMs = syncSendAtMs;
    reportSensorToSyncMs = sensorToSyncMs;
    reportInstAckRttMs = ackArrivedAtMs > syncSendAtMs ?
        ackArrivedAtMs - syncSendAtMs : 0;
    reportInstProcMs = instProcMs;
    resultReportHasInstProc = hasInstProc;
    if (hasInstProc && reportInstAckRttMs > instProcMs) {
        reportSyncToInstMs = (reportInstAckRttMs - instProcMs) / 2;
    } else {
        reportSyncToInstMs = reportInstAckRttMs / 2;
    }
    reportSyncProcessMs = syncProcessMs;
    reportTotalEstMs = reportSensorToSyncMs + reportSyncProcessMs + reportSyncToInstMs;
    reportFirstInstAckMs = ackArrivedAtMs;
    reportInstAckCount = instAckCount;
    reportInstArrivalDeltaMs = calculateInstArrivalDeltaMs();
    resultReportHasSensorMetric = sensorMetricReceived;
    resultReportCreatedAtMs = ackArrivedAtMs;
    resultReportPending = true;

    strncpy(ignoredInstCommand, activeInstCommand, sizeof(ignoredInstCommand) - 1);
    ignoredInstCommand[sizeof(ignoredInstCommand) - 1] = 0;
    ignoredSeq = activeSeq;
    ignoreDuplicatesUntilMs = ackArrivedAtMs + DUPLICATE_WINDOW_MS;
    resetActiveMeasurement();
}

void updatePendingInstAck(const char *name, const char *command,
    unsigned int seq, unsigned long arrivedAtMs) {
    if (!resultReportPending || strcmp(command, reportInstCommand) != 0 ||
        seq != reportSeq) {
        return;
    }

    addInstAck(name, arrivedAtMs);
    reportInstAckCount = instAckCount;
    reportInstArrivalDeltaMs = calculateInstArrivalDeltaMs();
}

void sendInstPacket(const char *payload, bool broadcastRoute) {
    if (broadcastRoute) {
        sync.broadcastUdpFast(payload);
    } else {
        sync.sendUdpToInstFast(payload);
    }
}

void sendInstBurst(const char *payload, bool broadcastRoute, unsigned long receivedAtMs) {
    for (int i = 0; i < INST_SEND_RETRY_COUNT; i++) {
        if (i == 0) {
            syncSendAtMs = millis();
            syncProcessMs = syncSendAtMs - receivedAtMs;
        }
        sendInstPacket(payload, broadcastRoute);
        if (i == 0) {
            sendSensorResult('K', activeCommand, activeSeq);
        }
        if (i < INST_SEND_RETRY_COUNT - 1 && INST_SEND_GAP_MS > 0) {
            delay(INST_SEND_GAP_MS);
        }
    }
}

void handleSensorCommand(char command, unsigned int seq, int bpm, unsigned long receivedAtMs) {
    char instCommand[16];
    bool broadcastRoute = false;

    if (command == 'S') {
        strncpy(instCommand, "START", sizeof(instCommand) - 1);
        instCommand[sizeof(instCommand) - 1] = 0;
    } else if (command == 'X') {
        strncpy(instCommand, "STOP", sizeof(instCommand) - 1);
        instCommand[sizeof(instCommand) - 1] = 0;
        broadcastRoute = true;
    } else if (command >= '1' && command <= '3') {
        snprintf(instCommand, sizeof(instCommand), "LEVEL:%c", command);
        broadcastRoute = true;
    } else if (command == 'B') {
        snprintf(instCommand, sizeof(instCommand), "BPM:%d", bpm);
        broadcastRoute = true;
    } else {
        return;
    }

    char payload[24];
    snprintf(payload, sizeof(payload), "%s|%u", instCommand, seq);

    startMeasurement(command, seq, instCommand,
        broadcastRoute ? "broadcast" : "unicast", receivedAtMs);
    sendInstBurst(payload, broadcastRoute, receivedAtMs);
}

void maybeExpireMeasurement() {
    if (!measurementActive) {
        return;
    }
    if ((unsigned long)(millis() - measurementStartedAtMs) > MEASUREMENT_TIMEOUT_US / 1000UL) {
        Serial.print("TIMEOUT ");
        Serial.print(activeCommand);
        Serial.print("#");
        Serial.println(activeSeq);
        resetActiveMeasurement();
    }
}

void receivePackets() {
    int packetSize = ackUdp.parsePacket();
    while (packetSize > 0) {
        unsigned long arrivedAtMs = millis();
        char buf[32];
        int len = ackUdp.read(buf, sizeof(buf) - 1);
        if (len > 0) {
            buf[len] = 0;
            while (len > 0 &&
                (buf[len - 1] == '\r' || buf[len - 1] == '\n' || buf[len - 1] == ' ')) {
                buf[--len] = 0;
            }

            char command = 0;
            int bpm = 0;
            char instName[8];
            char instCommand[16];
            unsigned int seq = 0;
            unsigned long metricMs = 0;
            unsigned long instProcMs = 0;
            bool hasInstProc = false;
            char *childName = nullptr;

            if (parseHello(buf, childName)) {
                sync.registerUdpChild(String(childName), ackUdp.remoteIP());
            } else if (parseSensorCommand(buf, command, seq, bpm)) {
                if (sync.ready == 2) {
                    handleSensorCommand(command, seq, bpm, arrivedAtMs);
                }
            } else if (parseSensorMetric(buf, command, seq, metricMs)) {
                updateReportSensorMetric(command, seq, metricMs);
            } else if (parseInstAck(buf, instName, sizeof(instName),
                instCommand, sizeof(instCommand), seq, instProcMs, hasInstProc)) {
                updatePendingInstAck(instName, instCommand, seq, arrivedAtMs);
                bool duplicateOfCompleted = strcmp(instCommand, ignoredInstCommand) == 0 &&
                    seq == ignoredSeq &&
                    (long)(ignoreDuplicatesUntilMs - arrivedAtMs) > 0;
                if (!duplicateOfCompleted && measurementActive &&
                    strcmp(instCommand, activeInstCommand) == 0 &&
                    seq == activeSeq && syncSendAtMs > 0) {
                    addInstAck(instName, arrivedAtMs);
                    finalizeMeasurement(arrivedAtMs, instProcMs, hasInstProc);
                }
            }
        }
        packetSize = ackUdp.parsePacket();
    }

    maybeExpireMeasurement();
}

void printPendingReports() {
    if (!resultReportPending) {
        return;
    }
    if (!resultReportHasSensorMetric &&
        (unsigned long)(millis() - resultReportCreatedAtMs) < REPORT_WAIT_TIMEOUT_MS) {
        return;
    }
    Serial.print("RESULT ");
    Serial.print(reportCommand);
    Serial.print("#");
    Serial.print(reportSeq);
    Serial.print(" payload=");
    Serial.print(reportInstCommand);
    Serial.print(" route=");
    Serial.print(reportRoute);
    Serial.print(" sync_recv_ms=");
    Serial.print(reportSyncRecvMs);
    Serial.print(" sync_send_ms=");
    Serial.print(reportSyncSendMs);
    Serial.print(" sensor_to_sync_ms=");
    if (resultReportHasSensorMetric) {
        Serial.print(reportSensorToSyncMs);
    } else {
        Serial.print("N/A");
    }
    Serial.print(" sync_process_ms=");
    Serial.print(reportSyncProcessMs);
    Serial.print(" sync_to_inst_ms=");
    Serial.print(reportSyncToInstMs);
    Serial.print(" inst_proc_ms=");
    if (resultReportHasInstProc) {
        Serial.print(reportInstProcMs);
    } else {
        Serial.print("N/A");
    }
    Serial.print(" total_est_ms=");
    if (resultReportHasSensorMetric) {
        Serial.print(reportTotalEstMs);
    } else {
        Serial.print("N/A");
    }
    Serial.print(" inst_ack_count=");
    Serial.print(reportInstAckCount);
    Serial.print(" inst_arrival_delta_ms=");
    if (reportInstAckCount >= 2) {
        Serial.print(reportInstArrivalDeltaMs);
    } else {
        Serial.print("N/A");
    }
    Serial.print(" inst_ack_rtt_ms=");
    Serial.println(reportInstAckRttMs);
    resultReportPending = false;
}

void printStatus() {
    unsigned long now = millis();
    if (now - lastStatusMs < 2000UL) {
        return;
    }
    lastStatusMs = now;
    Serial.print("SYNC_STATUS ready=");
    Serial.print(sync.ready);
    Serial.print(" active=");
    Serial.println(measurementActive ? 1 : 0);
}

void setup() {
    sync.Starts();
    ackUdp.begin(ACK_PORT);
    Serial.println("SyncMain: Minimal Delay Test Ready");
}

void loop() {
    receivePackets();

    if (sync.ready == 1) {
        sync.sendReady();
        sync.ready = 2;
    }

    printPendingReports();
    printStatus();
}
