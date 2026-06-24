#include "Sensfunc.h"
#include "config.h"
#include <WiFiUdp.h>

/*
 * SensClient 合計遅延テスト用スケッチ (Loopback方式)
 */

SensClass sens;
WiFiUDP udp;
unsigned long sendTime = 0;
bool waitingForLoopback = false;
unsigned long bootAt = 0;
bool connectionReady = false;

void sendAck(char command) {
    IPAddress syncIP;
    syncIP.fromString(SERVER_IP);
    char ack[3] = {'A', 's', command};
    udp.beginPacket(syncIP, ACK_PORT);
    udp.write((uint8_t*)ack, sizeof(ack));
    udp.endPacket();
}

void setup() {
    sens.Starts();
    udp.begin(UDP_PORT); // 楽器と同じポートを監視
    bootAt = millis();
    Serial.println("================================");
    Serial.println("SensClient: Total Delay Test (Loopback)");
    Serial.println("Input 's', 'r' or '1'-'3' to measure delay");
    Serial.println("================================");
}

void loop() {
    // 1. 同期機からのUDPループバックを監視
    int packetSize = udp.parsePacket();
    if (packetSize) {
        unsigned long recvTime = millis();
        char buf[256];
        int len = udp.read(buf, 255);
        if (len > 0) buf[len] = 0;

        String message(buf);
        message.trim();
        if (message == "START") sendAck('S');
        else if (message == "ROUND") sendAck('R');
        else if (message.startsWith("LEVEL:") && message.length() > 6) {
            sendAck(message.charAt(6));
        }
        
        if (waitingForLoopback) {
            Serial.print(">>> TOTAL DELAY (Sens -> Sync -> Sens): ");
            Serial.print(recvTime - sendTime);
            Serial.print(" ms (Message: ");
            Serial.print(buf);
            Serial.println(")");
            waitingForLoopback = false;
        }
    }

    if (sens.ready == 0){
        if (!connectionReady && millis() - bootAt >= INITIAL_CONNECT_DELAY_MS) {
            connectionReady = true;
        }
        if (connectionReady) sens.connection();
    } 
    else {
        // コマンド送信と送信時刻の記録
        if (Serial.available() > 0) {
            char cmd = Serial.read();
            if (cmd == 's' || cmd == 'r' || cmd == '1' || cmd == '2' || cmd == '3') {
                sendTime = millis();
                waitingForLoopback = true;
                
                if (cmd == 's') {
                    sens.sendStart();
                } else if (cmd == 'r') {
                    sens.sendRoundStart();
                } else {
                    sens.sendSpeed(cmd - '0');
                }
            }
        }
    }
}
