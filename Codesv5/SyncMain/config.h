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

// STARTを送るInstの順番です。最初の要素はセンサからのSTART受信直後、
// 2要素目以降はSTART_INTERVAL_BEATS拍ごとに個別にunicastします。
// 1つの要素に複数台を指定する場合は "inst1,inst2" のように記述します。
static const char* const PLAYBACK_TARGET_GROUPS[] = {
    "inst1"
};
#define PLAYBACK_TARGET_GROUP_COUNT \
    (sizeof(PLAYBACK_TARGET_GROUPS) / sizeof(PLAYBACK_TARGET_GROUPS[0]))

// 輪唱の開始間隔（8拍 = 4拍子の2小節）
#define START_INTERVAL_BEATS 8.0f

// BPM設定。LEVELは後方互換用に代表BPMへ変換します。
static const int LEVEL_BPM[] = {80, 100, 120};
#define DEFAULT_LEVEL 2
#define DEFAULT_BPM 100
#define MIN_BPM 40
#define MAX_BPM 240

// 接続を待つ子機の総数 (センサ 1 + 楽器 N)
// 楽器なしのテスト時はここを 1 に設定してください
#define numchild 2

#endif
