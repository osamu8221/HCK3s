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
    unsigned long serialWaitStart = millis();
    while (!Serial && millis() - serialWaitStart < 2000);
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
    static bool identificationSent = false;
    static int resendCount = 0;
    static unsigned long lastConnectAttemptMs = 0;
    static unsigned long lastIdentificationMs = 0;

    String resp = receiveMsg(client);
    while (resp.length() > 0) {
        if (resp == "WELCOME") {
            isRegistered = true;
            identificationSent = true;
            Serial.println("Server registered this device (WELCOME).");
        } else if (resp == "READY") {
            isRegistered = true;
            ready = 1;
            Serial.println("全子機の準備が整いました (READY受信)。演奏指示が可能になりました。");
            return;
        }
        resp = receiveMsg(client);
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

String SensClass::receiveMsg(WiFiClient &c) {
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

// 新機能の実装

void SensClass::sendStart() {
    if (this->client.connected()) {
        Serial.println("親機へ初回演奏開始指示 (START) を送信");
        this->client.print("START\n");
    }
}

void SensClass::sendStop() {
    if (this->client.connected()) {
        Serial.println("親機へ演奏停止指示 (STOP) を送信");
        this->client.print("STOP\n");
    }
}

void SensClass::sendSpeed(int speed) {
    if (this->client.connected()) {
        Serial.print("親機へ速度レベル ("); Serial.print(speed); Serial.println(") を送信");
        this->client.print(String(speed) + "\n");
    }
}

void SensClass::sendBpm(int bpm) {
    if (bpm < MIN_BPM || bpm > MAX_BPM) {
        Serial.print("無効なBPM: ");
        Serial.println(bpm);
        return;
    }
    if (this->client.connected()) {
        Serial.print("親機へBPM ("); Serial.print(bpm); Serial.println(") を送信");
        this->client.print("BPM:");
        this->client.print(bpm);
        this->client.print("\n");
    }
}
