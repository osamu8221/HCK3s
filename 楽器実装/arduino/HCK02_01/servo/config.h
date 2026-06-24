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

// この機の名前。Codes 2 と同様に親機へTCP登録するため、
//   親機 SyncMain の namechild = {sens, inst1, inst2, inst3, inst4} の中の
//   「他機と重複しない空きスロット名」にすること。
//   ★violin=inst1 / giro=inst2 を使っている前提で、servoは inst3 を割り当てる。
//   ※servoは登録スロットを1つ占有するが、可視化のため START/ROUND は全声部に反応する。
//   ※親機 SyncMain の numchild は、TCP登録する子機の総数(sens + 各inst)に合わせること。
#define myname "inst3"

#endif
