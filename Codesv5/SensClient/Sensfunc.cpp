#include "Sensfunc.h"

SensClass::SensClass() : lastHelloMs(0), isRegistered(false),ready(0) {
    serverIP.fromString(SERVER_IP);
}

void SensClass::WiFiStart() {
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

void SensClass::Starts(){
    Serial.begin(BAUD);
    while (!Serial);
    Serial.print(myname);
    Serial.println("起動");
    WiFiStart();
    startUDP();

}

void SensClass::startUDP() {
    udp.begin(UDP_PORT);
    Serial.println("UDPポート待機中");
}

bool SensClass::connectToServer() {
    if (client.connect(serverIP, TCP_PORT)) {
        Serial.println("Connected to server.");
        return true;
    }
    return false;
}

void SensClass::sendIdentification(const char* name) {
    client.println(name);
}

void SensClass::connection(){
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
            Serial.println("全子機の準備が整いました (READY受信)。演奏指示が可能になりました。");
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

String SensClass::receiveMsg(WiFiClient &c) {
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
        delay(1);
    }
    return "";
}

String SensClass::receiveUDP(WiFiUDP &u) {
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

// 新機能の実装

void SensClass::sendStart() {
    if (this->ready >= 1) {
        Serial.println("親機へ初回演奏開始指示 (START) を送信");
        udp.beginPacket(serverIP, UDP_PORT);
        udp.print("START");
        udp.endPacket();
    }
}

void SensClass::sendStop() {
    if (this->ready >= 1) {
        Serial.println("親機へ演奏停止指示 (STOP) を送信");
        udp.beginPacket(serverIP, UDP_PORT);
        udp.print("STOP");
        udp.endPacket();
    }
}

void SensClass::sendSpeed(int speed) {
    if (this->ready >= 1) {
        Serial.print("親機へ速度レベル ("); Serial.print(speed); Serial.println(") を送信");
        udp.beginPacket(serverIP, UDP_PORT);
        udp.print(String(speed));
        udp.endPacket();
    }
}

void SensClass::sendBpm(int bpm) {
    if (bpm < MIN_BPM || bpm > MAX_BPM) {
        Serial.print("無効なBPM: ");
        Serial.println(bpm);
        return;
    }
    if (this->ready >= 1) {
        Serial.print("親機へBPM ("); Serial.print(bpm); Serial.println(") を送信");
        udp.beginPacket(serverIP, UDP_PORT);
        udp.print("BPM:");
        udp.print(bpm);
        udp.endPacket();
    }
}
