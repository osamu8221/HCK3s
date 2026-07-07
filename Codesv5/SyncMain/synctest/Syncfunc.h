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
        bool childRegistered[5];
        IPAddress childIPs[5];
        
        int numprepered;
        size_t nextTargetGroupIndex;
        const char* namechild[5];
        bool sequenceStarted;
        float accumulatedBeats;
        unsigned long lastBeatUpdateMs;
        int currentLevel;
        int currentBpm;
        

    public:
        SyncClass();
        int ready;
        void APStart();
        void TCPStart();
        void UDPStart();
        void Starts();
        void connection();
        bool registerUdpChild(String childName, IPAddress childIP);
        void broadcastUDP(String message);
        bool unicastUDP(String childName, String message);
        bool sendUdpToSens(String message);
        bool sendUdpToSensFast(const char *message);
        bool sendUdpToInstFast(const char *message);
        bool broadcastUdpFast(const char *message);
        String recieve(WiFiClient &c);
        String receiveSensorCommand();

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
        void sendUdpCommand(String message);
        void sendUdpCommandFast(String message);
};
#endif
