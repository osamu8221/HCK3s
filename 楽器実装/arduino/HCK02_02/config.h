#ifndef CONFIG_H
#define CONFIG_H

// WiFi設定（親機のAP設定に合わせる / Codes 2 と統一）
#define secret_ssid "H1-SyncAP"
#define secret_pass "sync2026"
#define BAUD 115200

// 親機の情報（Codes 2 SyncMain と統一）
#define SERVER_IP "192.168.4.1"
#define TCP_PORT 9000
#define UDP_PORT 9001
#define INITIAL_CONNECT_DELAY_MS 1500
#define CONNECTION_RETRY_DELAY_MS 250
#define DEFAULT_LEVEL 2          // STOP後/起動時の既定レベル(Codes 3 と統一)

// この楽器(子機)の名前。親機 SyncMain の namechild に存在する名前にすること。
//   namechild = {sens, inst1, inst2, inst3, inst4}
//   ★ボードごとに重複しない名前へ変更する (violin担当なら inst1 など)。
#define myname "inst1"

#endif
