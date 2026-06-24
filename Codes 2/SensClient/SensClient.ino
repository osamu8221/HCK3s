#include "Sensfunc.h"
#include "config.h"
SensClass sens;
unsigned long bootAt = 0;
bool connectionReady = false;

void setup() {
    sens.Starts();
    bootAt = millis();
    Serial.println("================================");
    Serial.println("SensClient: Production Mode");
#if ENABLE_SERIAL_COMMAND
    Serial.println("Input 's', 'r' or '1'-'3' to send commands");
#endif
    Serial.println("================================");
}

void loop() {
    if (sens.ready == 0){
        if (!connectionReady && millis() - bootAt >= INITIAL_CONNECT_DELAY_MS) {
            connectionReady = true;
        }
        if (connectionReady) sens.connection();
    } 
    else {
#if ENABLE_SERIAL_COMMAND
        if (Serial.available() > 0) {
            char cmd = Serial.read();
            if (cmd == 's' || cmd == 'r' || cmd == '1' || cmd == '2' || cmd == '3') {
                if (cmd == 's') {
                    sens.sendStart();
                } else if (cmd == 'r') {
                    sens.sendRoundStart();
                } else {
                    sens.sendSpeed(cmd - '0');
                }
            }
        }
#endif
    }
}
