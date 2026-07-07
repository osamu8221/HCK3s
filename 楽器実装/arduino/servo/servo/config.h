#ifndef CONFIG_H
#define CONFIG_H

// WiFi設定（親機のAP設定に合わせる / Codes と統一）
#define secret_ssid "H1-SyncAP"
#define secret_pass "sync2026"
#define BAUD 115200

// 親機の情報（Codes SyncMain と統一）
#define SERVER_IP "192.168.4.1"
#define TCP_PORT 9000            // 互換のため残置(現行の登録はUDP HELLO)
#define UDP_PORT 9001
#define INITIAL_CONNECT_DELAY_MS 1500
#define CONNECTION_RETRY_DELAY_MS 250        // 未登録時の HELLO 再送間隔
#define REGISTERED_HELLO_RETRY_DELAY_MS 2000 // 登録後(READY待ち)の HELLO 再送間隔
#define DEFAULT_LEVEL 2          // STOP後/起動時の既定レベル(Codes と統一)

#define DEFAULT_BPM 100          // BPM未受信時の既定テンポ(親機 Codesv5 と統一)
#define MIN_BPM 40               // BPM:コマンドの下限
#define MAX_BPM 240              // BPM:コマンドの上限

// この機の名前。Codes と同様に親機へUDP HELLO で登録するため、
//   親機 SyncMain の namechild = {sens, inst1, inst2, inst3, inst4} の中の
//   「他機と重複しない空きスロット名」にすること。
//   ★violin=inst1 / giro=inst2 を使っている前提で、servoは inst3 を割り当てる。
//   ※servoは登録スロットを1つ占有する。可視化のため START は全声部に反応する。
//   ※親機 SyncMain の numchild は、登録する子機の総数(sens + 各inst)に合わせること。
#define myname "inst3"

#endif
