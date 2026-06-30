#include "Syncfunc.h"

SyncClass::SyncClass() : server(TCP_PORT), ready(0), numprepered(0) {
    namechild[0] = "sens";
    namechild[1] = "inst1";
    namechild[2] = "inst2";
    namechild[3] = "inst3";
    namechild[4] = "inst4";
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
    TCPStart();
    UDPStart();
}

void SyncClass::connection() {
    // 接続が切れているソケットを掃除
    for (int i = 0; i < 5; i++) {
        if (this->childClients[i] && !this->childClients[i].connected()) {
            this->childClients[i].stop();
        }
    }

    WiFiClient incomingClient = server.accept();
    if (incomingClient) {
        Serial.println("TCP接続を受理しました。識別名を待機します...");
        incomingClient.setTimeout(1000);
        String ClientName = incomingClient.readStringUntil('\n');
        ClientName.trim();
        if (ClientName == "") {
            Serial.println("識別名の受信がタイムアウトしたため切断します。");
            incomingClient.stop();
            return;
        }
        Serial.print("受信メッセージ: "); Serial.println(ClientName);
            
        int childIndex = -1;
        for (int i = 0; i < numchild; i++) {
            if (String(this->namechild[i]) == ClientName) {
                childIndex = i;
                break;
            }
        }
        
        if (childIndex != -1) {
            if (this->childClients[childIndex]) {
                this->childClients[childIndex].stop();
            }
            this->childClients[childIndex] = incomingClient;
            
            int count = 0;
            for (int i = 0; i < numchild; i++) {
                if (this->childClients[i] && this->childClients[i].connected()) {
                    count++;
                }
            }
            this->numprepered = count;

            this->childClients[childIndex].print("WELCOME\n");
            Serial.print("子機登録成功: "); Serial.print(ClientName);
            Serial.print(" ("); Serial.print(this->numprepered);
            Serial.print("/"); Serial.print(numchild);
            Serial.println(")");
        } else {
            Serial.print("未登録の子機のため切断します: "); Serial.println(ClientName);
            incomingClient.stop();
        }
    }

    if (this->ready == 0 && this->numprepered >= numchild) {
        this->ready = 1;
        Serial.println("全ての準備が整いました。子機へ開始合図を送信します...");
    }
}

void SyncClass::broadcastUDP(String message) {
    IPAddress broadcastIP(192, 168, 4, 255);
    udp.beginPacket(broadcastIP, UDP_PORT);
    udp.print(message);
    udp.endPacket();
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
    if (this->childClients[0] && this->childClients[0].connected()) {
        if (this->childClients[0].available()) {
            unsigned long t_recv = millis();
            String msg = this->recieve(this->childClients[0]);
            if (msg == "START") {
                this->sendStart();
            } else if (msg == "ROUND") {
                this->sendRoundStart();
            } else if (msg == "1" || msg == "2" || msg == "3") {
                this->sendLevel(msg.toInt());
            }
            unsigned long t_sent = millis();
            Serial.print(">>> RELAY DELAY: "); Serial.print(t_sent - t_recv); Serial.println(" ms");
        }
    }
}

void SyncClass::sendReady() {
    Serial.println("全子機へ READY を送信します...");
    for (int i = 0; i < numchild; i++) {
        if (this->childClients[i] && this->childClients[i].connected()) {
            this->childClients[i].print("READY\n");
        }
    }
}

void SyncClass::sendStart() {
    for (int i = 0; i < UDP_RETRY_COUNT; i++) {
        this->broadcastUDP("START");
    }
}

void SyncClass::sendRoundStart() {
    for (int i = 0; i < UDP_RETRY_COUNT; i++) {
        this->broadcastUDP("ROUND");
    }
}

void SyncClass::sendLevel(int level) {
    String msg = "LEVEL:" + String(level);
    for (int i = 0; i < UDP_RETRY_COUNT; i++) {
        this->broadcastUDP(msg);
    }
}
