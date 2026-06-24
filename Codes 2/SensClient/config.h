#ifndef CONFIG_H
#define CONFIG_H

// WiFi設定（親機のAP設定に合わせる）
#define secret_ssid "H1-SyncAP"
#define secret_pass "sync2026"
#define BAUD 115200

// 親機の情報
#define SERVER_IP "192.168.4.1"
#define TCP_PORT 9000
#define INITIAL_CONNECT_DELAY_MS 1500
#define CONNECTION_RETRY_DELAY_MS 250
#define ENABLE_SERIAL_COMMAND 1

//子機の情報
#define myname "sens"

#endif
