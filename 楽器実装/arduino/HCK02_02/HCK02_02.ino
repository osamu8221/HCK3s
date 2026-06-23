// --- 楽器用Arduino (バイオリン担当 / カエルの歌は輪唱曲) ---
// ボード: Arduino UNO R4 WiFi (オンボードESP32-S3 / WiFiS3ライブラリ)
//
// 役割:
//   ・同期用Arduinoから【WiFi UDPユニキャスト】で「再生の合図」と「テンポ情報(0〜4)」を受信
//     → ★情報伝達は共通モジュール SyncClient.h に統一(ギロ用inoと完全に同一)。
//   ・カエルの歌を【楽器ごとに声部を分けた輪唱】にして、
//     このArduinoが担当する1声部の音符を "Hz,ms" で Processing へ送信
//   ・Processing側は音色生成と再生・波形表示のみを担当する
//
// ★2026-06-18: 同期制御がブロードキャスト→ユニキャストに変更。受信専用の楽器は親機へ
//   自己登録(hello)を送ってIPを知らせる必要がある(SyncClientが起動時＋定期的に実施)。
//   ※UDPの受信ロジック自体は uni/broadcast 共通で不変。
//
// 輪唱の声部割り当て (ROUND_OFFSET=16ステップ=2小節):
//   VOICE_ID=0 (inst1) : 'A'と同時に第1声を開始
//   VOICE_ID=1 (inst2) : 'A'から16ステップ後に第2声を開始
//   VOICE_ID=2 (inst3) : 'A'から32ステップ後に第3声を開始
//   VOICE_ID=3 (inst4) : 'A'から48ステップ後に第4声を開始
//   ★このArduinoを書き込む機番に合わせて下の VOICE_ID を変更すること。
//
// 接続:
//   USB Serial (115200) -> このボード専用PCの violin.pde (バイオリンの音色・再生・波形)
//     ※「Arduino 1台 につき PC 1台」。各楽器は自分のPCでProcessingを動かす。
//   WiFi (UDP ユニキャスト) <- 同期用Arduino (自己登録したIP宛にコマンドが届く)
//
// 同期用Arduino から受信するUDPペイロード:
//   '0'〜'4' : テンポ情報。ino配列のbpmにリンクさせ、再生途中でも即時反映する。
//   'A'      : 音楽開始の合図。各機は自分の VOICE_ID に応じた遅延後に再生を始める。
//   'X'      : 停止合図。

#include "SyncClient.h"   // 共通: 情報伝達(WiFi接続/自己登録/コマンド受信)
#include "FrogMatrix.h"   // 2つのinoに共通: ドットのカエル(LEDマトリクス)

// ============================================================
// 通信設定: ギロ用inoと同じ値にそろえる。★は各自の環境に合わせる。
//   ★2026-06-18: 同期制御がブロードキャスト→ユニキャストに変更。
//     受信専用の楽器は親機へ自己登録(hello)を送り、IPを知らせる必要がある(SyncClientが実施)。
// ============================================================
const char* WIFI_SSID = "YOUR_SSID";              // ★
const char* WIFI_PASS = "YOUR_PASSWORD";          // ★
IPAddress   SYNC_HOST_IP(192, 168, 1, 100);       // ★同期制御(親機)のIP
const unsigned int SYNC_REG_PORT  = 50008;        // ★親機が自己登録(hello)を受けるポート
const unsigned int SYNC_RECV_PORT = 50007;        // この楽器が待ち受ける受信ポート
const char* NODE_NAME = "violin";                 // 自己登録で名乗る名前

// 書式: "音程(Hz),発音時間(ms)\n"
float melody[] = {
  523.2, 0.0, 587.3, 0.0, 659.2, 0.0, 698.4, 0.0,
  659.2, 0.0, 587.3, 0.0, 523.2, 0.0, 0.0, 0.0,
  659.2, 0.0, 698.4, 0.0, 784.0, 0.0, 880.0, 0.0,
  784.0, 0.0, 698.4, 0.0, 659.2, 0.0, 0.0, 0.0,
  523.2, 0.0, 0.0, 0.0,
  523.2, 0.0, 0.0, 0.0,
  523.2, 0.0, 0.0, 0.0,
  523.2, 0.0, 0.0, 0.0,
  523.2, 523.2, 587.3, 587.3, 659.2, 659.2, 698.4, 698.4,
  659.2, 0.0, 587.3, 0.0, 523.2, 0.0, 0.0, 0.0
};

int melodyLength = sizeof(melody) / sizeof(melody[0]);

// ============================================================
// 輪唱: 楽器ごとに声部を分担する。このArduinoの声部番号を VOICE_ID で指定する。
//   VOICE_ID=0 → inst1(第1声) 0=即時開始
//   VOICE_ID=1 → inst2(第2声) 16ステップ後から開始
//   VOICE_ID=2 → inst3(第3声) 32ステップ後から開始
//   VOICE_ID=3 → inst4(第4声) 48ステップ後から開始
// ★書き込む機番に合わせて VOICE_ID を 0〜3 に変更すること。
// ============================================================
const int VOICE_ID = 0;      // ★この機の声部番号 (0=inst1, 1=inst2, 2=inst3, 3=inst4)
const int ROUND_OFFSET = 16; // 声部間のずれ (8分音符×16 = 2小節)
long playPos = 0;            // 'A'からの経過ステップ(0開始)

// ============================================================
// テンポ: 同期側から来る 0〜4 と、ino配列のbpmをリンクさせる
//   8分音符の長さ(ms) = 30000 / bpm
//   0:100bpm(300ms) 1:120bpm(250ms) 2:140bpm(214ms)
//   3:160bpm(188ms) 4:180bpm(167ms)
// ============================================================
const int NUM_TEMPOS = 5;
const int tempoBpmTable[NUM_TEMPOS] = {100, 120, 140, 160, 180};
int tempoStage = 4;        // 既定: index1 = 120bpm
int tempoMs = 250;         // 8分音符の長さ(ms) = 30000 / bpm

bool playing = false;

// 非ブロッキングなシーケンサ用タイマ
unsigned long lastStepTime = 0;

SyncClient sync;            // 共通: 情報伝達
FrogMatrix frog;            // 共通: ドットのカエル

void setup() {
  Serial.begin(115200);     // -> このボード専用PCのProcessing

  frog.begin();             // LEDマトリクス初期化(まだ消灯)

  // WiFi接続→受信開始→親機へ自己登録(以降ハートビートで再登録)
  sync.begin(WIFI_SSID, WIFI_PASS, SYNC_HOST_IP,
             SYNC_REG_PORT, SYNC_RECV_PORT, NODE_NAME, processSyncByte);
}

void loop() {
  // 同期制御からの受信処理 + 自己登録ハートビート
  sync.update();

  // --- マスタークロックによる再生 ---
  // delay()ではなくmillis()で刻むので、再生途中のテンポ変更に即応できる。
  if (playing) {
    unsigned long now = millis();
    if (now - lastStepTime >= (unsigned long)tempoMs) {
      lastStepTime = now;
      sendCurrentVoices();
      playPos++; // 次のステップへ(メロディは剰余でループ＝無限カノン)
    }
  }

  // 共通: 音が流れている間「ドットのカエル」をループさせる(再生中のみ進む)
  frog.update();
}

// 受信した1バイトのコマンドを処理(SyncClientから呼ばれる)
void processSyncByte(char c) {
  if (c >= '0' && c <= '4') {
    // テンポ情報(0〜4): ino配列のbpmにリンク。tempoMsを即時更新するので、
    // 次のステップから新しいテンポで再生され、途中変更が成立する。
    tempoStage = c - '0';
    tempoMs = 30000 / tempoBpmTable[tempoStage];

  } else if (c == 'A') {
    // 音楽開始の合図: 全機が同時にカウント開始。各機は VOICE_ID 分だけ自動待機して入る。
    playing = true;
    playPos = 0;
    lastStepTime = millis();
    frog.start();          // ★音と同時にLEDマトリクスのカエルを開始

  } else if (c == 'X') {
    // 停止合図
    playing = false;
    frog.stop();           // 停止でカエル消灯
  }
}

// この機が担当する1声部の音符を Processing へ送信する。
// VOICE_ID * ROUND_OFFSET ステップ経過するまでは無音待機(sendVoiceNote内で pos<0 を弾く)。
void sendCurrentVoices() {
  int durationMs = (int)(tempoMs * 0.8);
  sendVoiceNote(playPos - VOICE_ID * ROUND_OFFSET, durationMs);
}

// 指定位置の音符を1行 "Hz,ms" で送信(休符・未開始は送らない)
void sendVoiceNote(long pos, int durationMs) {
  if (pos < 0) return;
  float pitch = melody[pos % melodyLength];
  if (pitch <= 0.0) return;       // 休符は送らない
  Serial.print(pitch);
  Serial.print(",");
  Serial.println(durationMs);     // 改行付きで送信(1声部=1行)
}
