#ifndef SENSFUNC_H
#define SENSFUNC_H

#include <Arduino.h>
#include <WiFiS3.h>
#include "config.h"

class SensClass{
    private:
        WiFiClient client;
        IPAddress serverIP;

    public:
        SensClass();
        bool isRegistered;
        int ready;
        void Starts();
        void WiFiStart();
        bool connectToServer();
        void connection();
        void sendIdentification(const char* name);
        String receiveMsg(WiFiClient &c);

        // 新機能
        void sendStart();
        void sendStop();
        void sendSpeed(int speed);
        void sendBpm(int bpm);

};
#endif
