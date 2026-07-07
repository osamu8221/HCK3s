#include "Syncfunc.h"

SyncClass::SyncClass()
    : server(TCP_PORT), numprepered(0), nextTargetGroupIndex(0),
      sequenceStarted(false), accumulatedBeats(0.0f), lastBeatUpdateMs(0),
      currentLevel(DEFAULT_LEVEL), currentBpm(DEFAULT_BPM), ready(0) {
    namechild[0] = "sens";
    namechild[1] = "inst1";
    namechild[2] = "inst2";
    namechild[3] = "inst3";
    namechild[4] = "inst4";
    for (int i = 0; i < 5; i++) {
        childRegistered[i] = false;
        childIPs[i] = IPAddress(0, 0, 0, 0);
    }
}

void SyncClass::APStart() {
    int status = WiFi.beginAP(secret_ssid, secret_pass);
    if (status != WL_AP_LISTENING) {
        Serial.println("エラー: APの起動に失敗しました。");
        while (true);
    }
    Serial.println("AP起動しました");
    Serial.print("SSID:"); Serial.println(secret_ssid);
    Serial.print("IPアドレス:"); Serial.println(WiFi.localIP());
}

void SyncClass::TCPStart() {
    server.begin();
    Serial.print("TCPサーバーを開始しました (ポート: ");
    Serial.print(TCP_PORT);
    Serial.println(")");
}

void SyncClass::UDPStart() {
    udp.begin(UDP_PORT);
    Serial.print("UDPポートを開放しました (ポート: ");
    Serial.print(UDP_PORT);
    Serial.println(")");
}

void SyncClass::Starts() {
    Serial.begin(BAUD);
    while (!Serial);
    Serial.println("起動しました");

    APStart();
    UDPStart();
}

void SyncClass::connection() {
    int packetSize = udp.parsePacket();
    if (packetSize) {
        char buf[256];
        buf[0] = 0;
        int len = udp.read(buf, 255);
        if (len > 0) buf[len] = 0;

        String msg(buf);
        msg.trim();
        IPAddress remoteIP = udp.remoteIP();

        const String helloPrefix = "HELLO|";
        if (msg.startsWith(helloPrefix)) {
            String ClientName = msg.substring(helloPrefix.length());
            ClientName.trim();

            if (ClientName.length() == 0) {
                Serial.println("空のUDP識別名を無視します。");
                return;
            }

            int childIndex = -1;
            for (int i = 0; i < numchild; i++) {
                if (String(this->namechild[i]) == ClientName) {
                    childIndex = i;
                    break;
                }
            }

            if (childIndex != -1) {
                bool newlyRegistered =
                    !this->childRegistered[childIndex] ||
                    this->childIPs[childIndex] != remoteIP;

                this->childIPs[childIndex] = remoteIP;
                this->childRegistered[childIndex] = true;

                int count = 0;
                for (int i = 0; i < numchild; i++) {
                    if (this->childRegistered[i]) {
                        count++;
                    }
                }
                this->numprepered = count;

                udp.beginPacket(remoteIP, UDP_PORT);
                udp.print(this->ready >= 1 ? "READY" : "WELCOME");
                udp.endPacket();

                if (newlyRegistered) {
                    Serial.print("受信メッセージ: ");
                    Serial.println(msg);
                    Serial.print("UDP子機登録成功: ");
                    Serial.print(ClientName);
                    Serial.print(" (");
                    Serial.print(this->numprepered);
                    Serial.print("/");
                    Serial.print(numchild);
                    Serial.println(")");
                }
            } else {
                Serial.print("未登録の子機HELLOを無視: ");
                Serial.println(ClientName);
            }
        } else if (this->ready == 2 && this->childRegistered[0] &&
                   remoteIP == this->childIPs[0]) {
            this->handleSensorMessage(msg);
        }
    }

    if (this->ready == 0 && this->numprepered >= numchild) {
        this->ready = 1;
        Serial.println("全ての準備が整いました。READY送信待ちへ進みます...");
    }
}

void SyncClass::handleSensorMessage(String msg) {
    if (msg == "START") {
        this->startSequence();
    } else if (msg == "STOP") {
        this->sendStop();
    } else if (msg == "1" || msg == "2" || msg == "3") {
        this->sendLevel(msg.toInt());
    } else if (msg.startsWith("BPM:") && msg.length() > 4) {
        this->sendBpm(msg.substring(4).toInt());
    }
}

String SyncClass::recieve(WiFiClient &c) {
    if (c.available()) {
        c.setTimeout(10); // タイムアウトを短く設定して高速化
        String line = c.readStringUntil('\n');
        line.trim();
        return line;
    }
    return "";
}

void SyncClass::recieveFrag() {
    int packetSize = udp.parsePacket();
    if (packetSize) {
        char buf[256];
        buf[0] = 0;
        int len = udp.read(buf, 255);
        if (len > 0) buf[len] = 0;

        String msg(buf);
        msg.trim();
        if (this->childRegistered[0] && udp.remoteIP() == this->childIPs[0]) {
            this->handleSensorMessage(msg);
        }
    }
}

void SyncClass::broadcastUDP(String message) {
    IPAddress broadcastIP(192, 168, 4, 255);
    udp.beginPacket(broadcastIP, UDP_PORT);
    udp.print(message);
    udp.endPacket();
}

bool SyncClass::unicastUDP(String childName, String message) {
    childName.trim();
    for (int i = 0; i < 5; i++) {
        if (childName == this->namechild[i]) {
            if (!this->childRegistered[i]) {
                Serial.print("送信対象が未接続です: ");
                Serial.println(childName);
                return false;
            }

            udp.beginPacket(this->childIPs[i], UDP_PORT);
            udp.print(message);
            return udp.endPacket() == 1;
        }
    }

    Serial.print("未登録の送信対象です: ");
    Serial.println(childName);
    return false;
}

void SyncClass::sendReady() {
    Serial.println("全子機へ READY を送信します...");
    for (int i = 0; i < numchild; i++) {
        if (this->childRegistered[i]) {
            udp.beginPacket(this->childIPs[i], UDP_PORT);
            udp.print("READY");
            udp.endPacket();
            Serial.print("READY送信: ");
            Serial.println(this->namechild[i]);
        }
    }
}

void SyncClass::startSequence() {
    // センサからのSTARTは初回だけ採用し、重複した開始を防ぎます。
    if (this->sequenceStarted) {
        return;
    }

    this->sequenceStarted = true;
    this->accumulatedBeats = 0.0f;
    this->lastBeatUpdateMs = millis();
    this->nextTargetGroupIndex = 0;
    this->sendStart();
}

void SyncClass::updateBeatClock() {
    if (!this->sequenceStarted) {
        return;
    }

    unsigned long now = millis();
    unsigned long elapsedMs = now - this->lastBeatUpdateMs;
    this->lastBeatUpdateMs = now;

    this->accumulatedBeats +=
        (static_cast<float>(elapsedMs) * static_cast<float>(this->currentBpm)) / 60000.0f;
}

void SyncClass::updateStartSchedule() {
    if (!this->sequenceStarted ||
        this->nextTargetGroupIndex >= PLAYBACK_TARGET_GROUP_COUNT) {
        return;
    }

    this->updateBeatClock();
    while (this->nextTargetGroupIndex < PLAYBACK_TARGET_GROUP_COUNT) {
        float targetBeat =
            static_cast<float>(this->nextTargetGroupIndex) * START_INTERVAL_BEATS;
        if (this->accumulatedBeats < targetBeat) {
            break;
        }
        this->sendStart();
    }
}

void SyncClass::sendStart() {
    if (this->nextTargetGroupIndex >= PLAYBACK_TARGET_GROUP_COUNT) {
        return;
    }

    String targets = PLAYBACK_TARGET_GROUPS[this->nextTargetGroupIndex];
    bool allSent = true;
    int targetCount = 0;
    int start = 0;

    while (start <= targets.length()) {
        int comma = targets.indexOf(',', start);
        int end = comma >= 0 ? comma : targets.length();
        String target = targets.substring(start, end);
        target.trim();

        if (target.length() > 0) {
            targetCount++;
            bool sent = false;
            for (int i = 0; i < UDP_RETRY_COUNT; i++) {
                if (this->unicastUDP(target, "START")) {
                    sent = true;
                }
            }
            if (!sent) allSent = false;
        }

        if (comma < 0) break;
        start = comma + 1;
    }

    this->nextTargetGroupIndex++;
    if (targetCount > 0 && allSent) {
        Serial.println("RELAY:START");
    }
}

void SyncClass::sendStop() {
    // 再START時に同じ順番を最初から実行できるよう、進行状態を初期化します。
    this->sequenceStarted = false;
    this->nextTargetGroupIndex = 0;
    this->accumulatedBeats = 0.0f;
    this->lastBeatUpdateMs = 0;
    this->currentLevel = DEFAULT_LEVEL;
    this->currentBpm = DEFAULT_BPM;

    for (int i = 0; i < UDP_RETRY_COUNT; i++) {
        this->broadcastUDP("STOP");
    }
    Serial.println("RELAY:STOP");
}

void SyncClass::sendLevel(int level) {
    if (level < 1 || level > 3) {
        return;
    }

    // 変更前のBPMでここまでの拍数を確定してから新しいLEVELへ切り替えます。
    this->updateBeatClock();
    this->currentLevel = level;
    this->currentBpm = LEVEL_BPM[level - 1];

    String msg = "LEVEL:" + String(level);
    for (int i = 0; i < UDP_RETRY_COUNT; i++) {
        this->broadcastUDP(msg);
    }
    Serial.print("RELAY:");
    Serial.println(msg);
}

void SyncClass::sendBpm(int bpm) {
    if (bpm < MIN_BPM || bpm > MAX_BPM) {
        return;
    }

    this->updateBeatClock();
    this->currentBpm = bpm;

    String msg = "BPM:" + String(bpm);
    for (int i = 0; i < UDP_RETRY_COUNT; i++) {
        this->broadcastUDP(msg);
    }
    Serial.print("RELAY:");
    Serial.println(msg);
}
