#ifndef SYNCFUNC_H
#define SYNCFUNC_H

#include <Arduino.h>
#include <WiFiS3.h>
#include <WiFiUdp.h>
#include "config.h"

class SyncClass{
    private:
        WiFiServer server;
        WiFiUDP udp;
        WiFiClient childClients[5];
        IPAddress childIPs[5];
        bool childRegistered[5];
        
        int numprepered;
        size_t nextTargetGroupIndex;
        const char* namechild[5];
        bool sequenceStarted;
        float accumulatedBeats;
        unsigned long lastBeatUpdateMs;
        int currentLevel;
        int currentBpm;
        void handleSensorMessage(String msg);
        

    public:
        SyncClass();
        int ready;
        void APStart();
        void TCPStart();
        void UDPStart();
        void Starts();
        void connection();
        void broadcastUDP(String message);
        bool unicastUDP(String childName, String message);
        String recieve(WiFiClient &c);

        // 新機能
        void recieveFrag();
        void sendReady();
        void startSequence();
        void updateBeatClock();
        void updateStartSchedule();
        void sendStart();
        void sendStop();
        void sendLevel(int level);
        void sendBpm(int bpm);


};
#endif
