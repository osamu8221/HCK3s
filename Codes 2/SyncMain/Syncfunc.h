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
        
        int numprepered;
        size_t nextTargetGroupIndex;
        const char* namechild[5];
        

    public:
        SyncClass();
        int ready;
        void APStart();
        void TCPStart();
        void UDPStart();
        void Starts();
        void connection();
        void broadcastUDP(String message);
        String recieve(WiFiClient &c);

        // 新機能
        void recieveFrag();
        void sendReady();
        void sendStart();
        void sendRoundStart();
        void sendLevel(int level);


};
#endif
