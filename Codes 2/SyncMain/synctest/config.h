#ifndef CONFIG_H
#define CONFIG_H

// WiFi設定
#define secret_ssid "H1-SyncAP"
#define secret_pass "sync2026"
#define BAUD 115200

// 通信ポート設定
#define TCP_PORT 9000
#define UDP_PORT 9001
#define ACK_PORT 9002
#define UDP_RETRY_COUNT 1

// 接続を待つ子機の総数 (センサ 1 + 楽器 N)
// 楽器なしのテスト時はここを 1 に設定してください
#define numchild 2

#endif
