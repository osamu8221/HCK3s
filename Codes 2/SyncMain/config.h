#ifndef CONFIG_H
#define CONFIG_H

// WiFi設定
#define secret_ssid "H1-SyncAP"
#define secret_pass "sync2026"
#define BAUD 115200

// 通信ポート設定
#define TCP_PORT 9000
#define UDP_PORT 9001
#define UDP_RETRY_COUNT 1

// 演奏へ参加させるInstの順番です。
// 1要素目はSTART、2要素目以降はROUNDごとの対象になります。
// 1つの要素に複数台を指定する場合は "inst1,inst2" のように記述します。
// 輪唱指示の出し方は現時点ではこの形式で暫定運用します。
// 対象指定やタイミングの仕様が明確になったら、このコメントと設定を更新してください。
static const char* const PLAYBACK_TARGET_GROUPS[] = {
    "inst1",
    "inst2"
};
#define PLAYBACK_TARGET_GROUP_COUNT \
    (sizeof(PLAYBACK_TARGET_GROUPS) / sizeof(PLAYBACK_TARGET_GROUPS[0]))

// 接続を待つ子機の総数 (センサ 1 + 楽器 N)
// 楽器なしのテスト時はここを 1 に設定してください
#define numchild 2

#endif
