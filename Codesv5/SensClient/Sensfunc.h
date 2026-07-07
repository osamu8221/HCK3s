#ifndef SENSFUNC_H
#define SENSFUNC_H

#include <Arduino.h>
#include <WiFiS3.h>
#include <WiFiUdp.h>
#include "config.h"

class SensClass{
    private:
        WiFiClient client;
        IPAddress serverIP;
        WiFiUDP udp;
        unsigned long lastHelloMs;

    public:
        SensClass();
        bool isRegistered;
        int ready;
        void Starts();
        void WiFiStart();
        void startUDP();
        bool connectToServer();
        void connection();
        void sendIdentification(const char* name);
        String receiveMsg(WiFiClient &c);
        String receiveUDP(WiFiUDP &u);

        // 新機能
        void sendStart();
        void sendStop();
        void sendSpeed(int speed);
        void sendBpm(int bpm);

};
#endif
