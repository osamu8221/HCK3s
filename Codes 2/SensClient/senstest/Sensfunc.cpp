#include "Sensfunc.h"

SensClass::SensClass() : isRegistered(false),ready(0) {
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
    Serial.print("Sent identification: ");
    Serial.println(name);
}

void SensClass::connection(){
    if (!client.connected()) {
        ready = 0;
        isRegistered = false;
        if (connectToServer()) {
            // 親機に名乗る
            sendIdentification(myname);
        } else {
            Serial.println("Connection failed. Retrying...");
            delay(CONNECTION_RETRY_DELAY_MS);
            return;
        }
    }
    if (!isRegistered){
        ready = 0;
        String resp = receiveMsg(client);
        if (resp == "WELCOME") {
            isRegistered = true;
            Serial.println("Server registered this device (WELCOME).");
        }
    }
    else if (ready == 0){
        String resp = receiveMsg(client);
        if (resp == "READY") {
            ready = 1;
            Serial.println("全子機の準備が整いました (READY受信)。演奏指示が可能になりました。");
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

// 新機能の実装

void SensClass::sendStart() {
    if (this->client.connected()) {
        Serial.println("親機へ演奏開始指示 (START) を送信");
        this->client.print("START\n");
    }
}

void SensClass::sendRoundStart() {
    if (this->client.connected()) {
        Serial.println("親機へ輪唱開始指示 (ROUND) を送信");
        this->client.print("ROUND\n");
    }
}

void SensClass::sendSpeed(int speed) {
    if (this->client.connected()) {
        Serial.print("親機へ速度レベル ("); Serial.print(speed); Serial.println(") を送信");
        this->client.print(String(speed) + "\n");
    }
}
