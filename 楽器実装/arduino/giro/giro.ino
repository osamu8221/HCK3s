// --- 楽器用Arduino (ギロ担当 / リズム) ---
// ボード: Arduino UNO R4 WiFi (オンボードESP32-S3 / WiFiS3ライブラリ)
//
// 役割:
//   ・同期用Arduinoから「再生の合図」と「テンポ情報(0〜4)」を【WiFi UDPユニキャスト】で受信
//     → ★情報伝達は共通モジュール SyncClient.h に統一(バイオリン用inoと完全に同一)。
//   ・ギロのビートパターン(リズム)を【このArduinoが保持】し、
//     各8分音符ステップの「擦る長さ(ms)」を giro.pde へシリアル送信する。
//     ※元は giro.pde にあったパターン情報を ino 側へ移動した。
//   ・giro.pde 側は音色生成・再生・波形表示のみを担当する。
//   ・「再生の合図」でオンボードLEDマトリクスの「ドットのカエル」を
//     音と同時に開始し、再生中はループさせる(2つのinoに共通=FrogMatrix.h)。
//
// ★2026-06-18: 同期制御がブロードキャスト→ユニキャストに変更。
//   受信専用の楽器は親機にIPを知らせる必要があるため、SyncClient が
//   起動時＋定期的に親機へ自己登録(hello)を送る。受信ロジック自体は不変。
//
// 接続:
//   USB Serial (115200) -> このボード専用PCの giro.pde
//   WiFi (UDP ユニキャスト) <- 同期用Arduino
//
// 受信コマンド(バイオリン用inoと共通): '0'〜'4' テンポ / 'A' 再生開始 / 'X' 停止

#include "SyncClient.h"   // 共通: 情報伝達(WiFi接続/自己登録/コマンド受信)
#include "FrogMatrix.h"   // 共通: ドットのカエル(LEDマトリクス)

// ============================================================
// 通信設定: バイオリン用inoと同じ値にそろえる。★は各自の環境に合わせる。
// ============================================================
const char* WIFI_SSID = "YOUR_SSID";              // ★
const char* WIFI_PASS = "YOUR_PASSWORD";          // ★
IPAddress   SYNC_HOST_IP(192, 168, 1, 100);       // ★同期制御(親機)のIP
const unsigned int SYNC_REG_PORT  = 50008;        // ★親機が自己登録(hello)を受けるポート
const unsigned int SYNC_RECV_PORT = 50007;        // この楽器が待ち受ける受信ポート
const char* NODE_NAME = "giro";                   // 自己登録で名乗る名前

// ============================================================
// ギロのビートパターン（1要素＝8分音符）。
//   値は「擦る長さ(係数)」: 0.8=長く擦る(チー) / 0.3=短く擦る(チッ) / 0.0=休符(ウン)
//   ※リズム情報は giro.pde には書かず、この ino が保持する。
// ============================================================
float beatPattern[] = {
  // 1小節目：チー・チッ・チー・チッ (表拍を強調)
  0.8, 0.3, 0.8, 0.3, 0.8, 0.3, 0.8, 0.3,
  // 2小節目：チー・チッ・チー・チッ
  0.8, 0.3, 0.8, 0.3, 0.8, 0.3, 0.8, 0.3,
  // 3小節目：チー・ウン・チッ・チッ (少しリズムを変える)
  0.8, 0.0, 0.3, 0.3, 0.8, 0.3, 0.8, 0.3,
  // 4小節目：チッ・チッ…(フィルイン：次の小節への繋ぎの連打)
  0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3
};
const int patternLength = sizeof(beatPattern) / sizeof(beatPattern[0]);

// ============================================================
// テンポ: 同期側から来る 0〜4 と bpm をリンク(バイオリン用inoと共通の表)。
//   8分音符の長さ(ms) = 30000 / bpm
// ============================================================
const int NUM_TEMPOS = 5;
const int tempoBpmTable[NUM_TEMPOS] = {100, 120, 140, 160, 180};
int tempoStage = 1;        // 既定: 120bpm
int tempoMs = 250;         // 8分音符の長さ(ms) = 30000 / bpm

bool playing = false;
long stepPos = 0;          // 'A'からの経過ステップ(0開始, 剰余でパターンをループ)
unsigned long lastStepTime = 0;

SyncClient sync;           // 共通: 情報伝達
FrogMatrix frog;           // 共通: ドットのカエル

void setup() {
  Serial.begin(115200);    // -> このボード専用PCの giro.pde

  frog.begin();            // LEDマトリクス初期化(まだ消灯)

  // WiFi接続→受信開始→親機へ自己登録(以降ハートビートで再登録)
  sync.begin(WIFI_SSID, WIFI_PASS, SYNC_HOST_IP,
             SYNC_REG_PORT, SYNC_RECV_PORT, NODE_NAME, processSyncByte);
}

void loop() {
  // 同期制御からの受信処理 + 自己登録ハートビート
  sync.update();

  // --- マスタークロックによるリズム再生(millis()で非ブロッキング) ---
  if (playing) {
    unsigned long now = millis();
    if (now - lastStepTime >= (unsigned long)tempoMs) {
      lastStepTime = now;
      sendCurrentBeat();
      stepPos++; // 次のステップへ(パターンは剰余でループ)
    }
  }

  // 共通: 音が流れている間「ドットのカエル」をループさせる(再生中のみ進む)
  frog.update();
}

// 受信した1バイトのコマンドを処理(SyncClientから呼ばれる) ── バイオリン用inoと共通の合図
void processSyncByte(char c) {
  if (c >= '0' && c <= '4') {
    // テンポ情報: tempoMsを即時更新 → 次ステップから新テンポ(途中変更に即応)
    tempoStage = c - '0';
    tempoMs = 30000 / tempoBpmTable[tempoStage];

  } else if (c == 'A') {
    // 再生開始の合図: リズム開始 + カエルを音と同時に開始
    playing = true;
    stepPos = 0;
    lastStepTime = millis();
    frog.start();          // ★音と同時にLEDマトリクスのカエルを開始

  } else if (c == 'X') {
    // 停止合図: リズム停止 + カエル消灯
    playing = false;
    frog.stop();
  }
}

// 現在ステップの「擦る長さ(ms)」を giro.pde へ送信(休符は送らない)。
//   duration[ms] = tempoMs × 係数  (元の giro.pde と同じ計算)
void sendCurrentBeat() {
  float coeff = beatPattern[stepPos % patternLength];
  if (coeff <= 0.0) return;                // 休符(ウン)は送らない
  int durationMs = (int)(tempoMs * coeff);
  Serial.println(durationMs);              // giro.pde は先頭フィールド=擦る長さ(ms)
}
