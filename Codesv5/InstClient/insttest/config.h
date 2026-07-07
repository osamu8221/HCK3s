#ifndef CONFIG_H
#define CONFIG_H

// WiFi設定（親機のAP設定に合わせる）
#define secret_ssid "H1-SyncAP"
#define secret_pass "sync2026"
#define BAUD 115200

// 親機の情報
#define SERVER_IP "192.168.4.1"
#define TCP_PORT 9000
#define UDP_PORT 9001
#define ACK_PORT 9002
#define INITIAL_CONNECT_DELAY_MS 100
#define CONNECTION_RETRY_DELAY_MS 200
#define IDENTIFICATION_RESEND_INTERVAL_MS 300
#define IDENTIFICATION_RESEND_LIMIT 2
#define DEFAULT_LEVEL 2
#define DEFAULT_BPM 100
#define MIN_BPM 40
#define MAX_BPM 240
static const int LEVEL_BPM[] = {80, 100, 120};

//子機の情報
#define myname "inst1"

#endif
