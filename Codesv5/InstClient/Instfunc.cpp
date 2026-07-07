#include "Instfunc.h"

InstClass::InstClass()
    : lastHelloMs(0), isRegistered(false), ready(0), currentLevel(DEFAULT_LEVEL),
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
    while (!Serial);
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
}

void InstClass::connection(){
    if (ready >= 1) {
        return;
    }

    String resp = receiveUDP(udp);
    while (resp.length() > 0) {
        if (resp == "WELCOME") {
            if (!isRegistered) {
                isRegistered = true;
                Serial.println("Server registered this device (WELCOME).");
            }
        } else if (resp == "READY") {
            isRegistered = true;
            ready = 1;
            Serial.println("全員の準備が整いました (READY受信)");
            return;
        }
        resp = receiveUDP(udp);
    }

    unsigned long now = millis();
    unsigned long retryDelay = this->isRegistered ?
        REGISTERED_HELLO_RETRY_DELAY_MS : CONNECTION_RETRY_DELAY_MS;
    if (now - this->lastHelloMs >= retryDelay) {
        bool firstHello = this->lastHelloMs == 0;
        udp.beginPacket(serverIP, UDP_PORT);
        udp.print("HELLO|");
        udp.print(myname);
        udp.endPacket();
        this->lastHelloMs = now;
        if (!this->isRegistered && firstHello) {
            Serial.print("UDP HELLO送信: ");
            Serial.println(myname);
        }
    }
}

String InstClass::receiveTCP(WiFiClient &c) {
    unsigned long start = millis();
    String line = "";
    c.setTimeout(2000);
    while (millis() - start < 2000) { // 2秒待機
        if (c.available()) {
            char ch = c.read();
            if (ch == '\n') {
                line.trim();
                if (line.length() > 0) return line;
            } else if (ch != '\r') {
                line += ch;
            }
        }
        delay(1); // CPU負荷を抑えつつWiFiチップの処理を待つ
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

    return 0;
}
