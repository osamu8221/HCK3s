#include "Instfunc.h"

InstClass::InstClass()
    : isRegistered(false), ready(0), currentLevel(DEFAULT_LEVEL),
      currentBpm(DEFAULT_BPM) {
    serverIP.fromString(SERVER_IP);
}

void InstClass::WiFiStart() {
    Serial.print("親機APに接続中: ");
    Serial.println(secret_ssid);
    WiFi.begin(secret_ssid, secret_pass);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi接続成功");
    serverIP.fromString(SERVER_IP);
}

void InstClass::startUDP() {
    udp.begin(UDP_PORT);
    Serial.println("UDPポート待機中");
}

void InstClass::Starts(){
    Serial.begin(BAUD);
    unsigned long serialWaitStart = millis();
    while (!Serial && millis() - serialWaitStart < 2000);
    Serial.print(myname);
    Serial.println("起動");
    WiFiStart();
    startUDP();

}

bool InstClass::connectToServer() {
    if (client.connect(serverIP, TCP_PORT)) {
        Serial.println("Connected to server.");
        return true;
    }
    return false;
}

void InstClass::sendIdentification(const char* name) {
    client.println(name);
    Serial.print("Sent identification: ");
    Serial.println(name);
}

void InstClass::connection(){
    static bool identificationSent = false;
    static int resendCount = 0;
    static unsigned long lastConnectAttemptMs = 0;
    static unsigned long lastIdentificationMs = 0;

    String resp = receiveTCP(client);
    while (resp.length() > 0) {
        if (resp == "WELCOME") {
            isRegistered = true;
            identificationSent = true;
            Serial.println("Server registered this device (WELCOME).");
        } else if (resp == "READY") {
            isRegistered = true;
            ready = 1;
            Serial.println("全員の準備が整いました (READY受信)");
            return;
        }
        resp = receiveTCP(client);
    }

    if (!client.connected()) {
        unsigned long now = millis();
        if (now - lastConnectAttemptMs < CONNECTION_RETRY_DELAY_MS) {
            return;
        }
        lastConnectAttemptMs = now;
        ready = 0;
        isRegistered = false;
        identificationSent = false;
        resendCount = 0;
        lastIdentificationMs = 0;
        if (connectToServer()) {
            sendIdentification(myname);
            identificationSent = true;
            lastIdentificationMs = millis();
        } else {
            Serial.println("Connection failed. Retrying...");
            return;
        }
    }

    if (client.connected() && !isRegistered && identificationSent &&
        millis() - lastIdentificationMs >= IDENTIFICATION_RESEND_INTERVAL_MS) {
        if (resendCount < IDENTIFICATION_RESEND_LIMIT) {
            sendIdentification(myname);
            resendCount++;
            lastIdentificationMs = millis();
        } else {
            Serial.println("Registration timeout. Reconnecting...");
            client.stop();
        }
    }
}

String InstClass::receiveTCP(WiFiClient &c) {
    static String line = "";
    while (c.available()) {
        char ch = c.read();
        if (ch == '\n') {
            line.trim();
            String message = line;
            line = "";
            if (message.length() > 0) return message;
        } else if (ch != '\r') {
            line += ch;
        }
    }
    return "";
}

String InstClass::receiveUDP(WiFiUDP &u) {
    int packetSize = u.parsePacket();
    if (packetSize) {
        char buf[256];
        buf[0] = 0;
        int len = u.read(buf, 255);
        if (len > 0) buf[len] = 0;
        String msg(buf);
        msg.trim();
        return msg;
    }
    return "";
}

String InstClass::recieveCommand() {
    return this->receiveUDP(this->udp);
}

bool InstClass::recieveCommandFast(char *buffer, size_t bufferSize) {
    int packetSize = this->udp.parsePacket();
    if (!packetSize || bufferSize == 0) {
        return false;
    }

    int len = this->udp.read(buffer, bufferSize - 1);
    if (len <= 0) {
        buffer[0] = 0;
        return false;
    }
    buffer[len] = 0;

    while (len > 0 &&
        (buffer[len - 1] == '\r' || buffer[len - 1] == '\n' || buffer[len - 1] == ' ')) {
        buffer[--len] = 0;
    }
    return len > 0;
}

// 新機能の実装

void InstClass::recieveStart() {
    String msg = this->receiveUDP(this->udp);
    if (msg == "START") {
        Serial.println("[UDP] 演奏開始合図を受信！");
        this->ready = 2; // 演奏状態へ
    }
}

int InstClass::recieveLevel() {
    String msg = this->receiveUDP(this->udp);
    if (msg.startsWith("LEVEL:")) {
        String val = msg.substring(6);
        int level = val.toInt();
        if (level >= 1 && level <= 3) {
            this->currentBpm = LEVEL_BPM[level - 1];
        }
        Serial.print("[UDP] 速度レベル受信: "); Serial.println(level);
        return level;
    }
    return 0; // 届いていない場合は0
}

int InstClass::recieveInstruction() {
    String msg = this->receiveUDP(this->udp);
    if (msg.length() == 0) {
        return 0;
    }

    if (msg.startsWith("LEVEL:")) {
        this->currentLevel = msg.substring(6).toInt();
        if (this->currentLevel >= 1 && this->currentLevel <= 3) {
            this->currentBpm = LEVEL_BPM[this->currentLevel - 1];
        }
        Serial.print("[UDP] 速度レベル受信: ");
        Serial.println(this->currentLevel);
        return this->currentLevel;
    }

    if (msg.startsWith("BPM:")) {
        int bpm = msg.substring(4).toInt();
        if (bpm >= MIN_BPM && bpm <= MAX_BPM) {
            this->currentBpm = bpm;
            Serial.print("[UDP] BPM受信: ");
            Serial.println(this->currentBpm);
            return this->currentBpm;
        }
    }

    if (msg == "ROUND") {
        Serial.println("[UDP] ROUND受信");
        return 0;
    }

    if (msg.startsWith("ROUND:")) {
        Serial.print("[UDP] ROUND受信: ");
        Serial.println(msg.substring(6).toInt());
    }
    return 0;
}
