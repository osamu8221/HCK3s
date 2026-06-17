// --- サーボモーター用Arduino (音階に連動して動く6機) ---
// ボード: Arduino UNO R4 WiFi (オンボードESP32-S3 / WiFiS3ライブラリ)
//
// 役割:
//   ・同期用Arduinoから【WiFi UDPブロードキャスト】で「再生の合図」と「テンポ情報(0〜4)」を受信
//   ・カエルの歌を【楽器ごとに声部を分けた輪唱(4声部)】でシミュレートし、
//     各ステップで鳴っている全4声部の「音階」に対応するサーボを動かす
//     (カエルの歌は ド・レ・ミ・ファ・ソ・ラ の6音 → サーボ6機に1対1で割り当て)
//
// 接続:
//   WiFi (UDP) <- 同期用Arduinoのブロードキャスト (全受信機が同一データを受信)
//   サーボ6機の信号線 = D2,D3,D4,D5,D6,D7
//     ※サーボの電源は外部5V電源から供給し、GNDをArduinoと共通にする。
//     ※USB Serialはデバッグ表示用(任意)。このボードはProcessingには繋がない。
//
// 同期用Arduino から受信するUDPコマンド(楽器用Arduinoと共通):
//   '0'〜'4' : テンポ情報。再生途中でも即時反映する。
//   'A'      : 音楽開始の合図。4声部を ROUND_OFFSET 刻みで順次開始する。
//   'X'      : 停止合図(全サーボを基準角へ戻す)。

#include <Servo.h>
#include <WiFiS3.h>

// ============================================================
// WiFi設定: 同期Arduinoと全受信機を同じネットワーク(同一サブネット)に入れる。
//   同期Arduinoは 255.255.255.255:SYNC_UDP_PORT にコマンドをブロードキャストする。
//   (楽器用Arduinoと同じSSID/PASS/PORTにすること)
// ============================================================
const char* WIFI_SSID = "YOUR_SSID";       // ★各自の環境に合わせる
const char* WIFI_PASS = "YOUR_PASSWORD";   // ★各自の環境に合わせる
const unsigned int SYNC_UDP_PORT = 50007;  // 楽器用Arduino・同期Arduinoと一致させる

WiFiUDP udp;
char udpBuf[32];

// 書式は楽器用Arduinoと同じメロディ(カエルの歌)。各要素＝8分音符。
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

// 楽器4台の輪唱(各機が1声部ずつ担当)を全て反映するため、4声部を ROUND_OFFSET 刻みで追う。
const int VOICE_COUNT  = 4;  // 楽器台数 = 輪唱の声部数
const int ROUND_OFFSET = 16; // 声部間のずれ (8分音符×16 = 2小節)
long playPos = 0;             // 'A'からの経過ステップ(0開始)

// ============================================================
// 音階 → サーボの対応 (ド・レ・ミ・ファ・ソ・ラ の6音)
//   NOTE_HZ[i] が鳴る音 → SERVO_PIN[i] のサーボが動く
// ============================================================
const int NUM_SERVOS = 6;
const float NOTE_HZ[NUM_SERVOS]  = {523.2, 587.3, 659.2, 698.4, 784.0, 880.0}; // ドレミファソラ
const int   SERVO_PIN[NUM_SERVOS] = {2, 3, 4, 5, 6, 7};
Servo servos[NUM_SERVOS];

const int REST_ANGLE   = 0;   // 待機角
const int ACTIVE_ANGLE = 70;  // 発音時に動かす角
// 各サーボを基準角へ戻す時刻(0=既に戻し済み)。非ブロッキングで戻すために使う。
unsigned long servoReturnAt[NUM_SERVOS];

// ============================================================
// テンポ: 同期側の 0〜4 と bpm をリンク (楽器用Arduinoと同一)
//   8分音符の長さ(ms) = 30000 / bpm
// ============================================================
const int NUM_TEMPOS = 5;
const int tempoBpmTable[NUM_TEMPOS] = {100, 120, 140, 160, 180};
int tempoStage = 1;
int tempoMs = 250;

bool playing = false;
unsigned long lastStepTime = 0;

void setup() {
  Serial.begin(115200);     // デバッグ表示用(任意)

  for (int i = 0; i < NUM_SERVOS; i++) {
    servos[i].attach(SERVO_PIN[i]);
    servos[i].write(REST_ANGLE);
    servoReturnAt[i] = 0;
  }

  connectWiFi();            // <- 同期Arduinoと同じネットワークに接続
  udp.begin(SYNC_UDP_PORT); // 同期ブロードキャストの受信開始
}

void loop() {
  handleSyncCommand();   // テンポ変更・開始/停止合図を常時監視
  releaseServos();       // 発音時間が過ぎたサーボを基準角へ戻す(非ブロッキング)

  // --- マスタークロックによるステップ送り ---
  // millis()で刻むので、再生途中のテンポ変更にも即応する。
  if (playing) {
    unsigned long now = millis();
    if (now - lastStepTime >= (unsigned long)tempoMs) {
      lastStepTime = now;
      stepServos();
      playPos++; // 次のステップへ(メロディは剰余でループ)
    }
  }
}

// WiFi接続(接続できるまで待つ)
void connectWiFi() {
  int status = WL_IDLE_STATUS;
  while (status != WL_CONNECTED) {
    status = WiFi.begin(WIFI_SSID, WIFI_PASS);
    unsigned long t0 = millis();
    while (status != WL_CONNECTED && millis() - t0 < 8000) {
      delay(300);
      status = WiFi.status();
    }
  }
}

// 同期Arduinoからのコマンド受信(UDPブロードキャスト, 楽器用Arduinoと共通仕様)
void handleSyncCommand() {
  int packetSize = udp.parsePacket();
  if (packetSize <= 0) return;

  int n = udp.read(udpBuf, sizeof(udpBuf));
  for (int i = 0; i < n; i++) {
    processSyncByte(udpBuf[i]);
  }
}

// 受信した1バイトのコマンドを処理
void processSyncByte(char c) {
  if (c >= '0' && c <= '4') {
    // テンポ情報(0〜4): tempoMsを即時更新 → 次ステップから新テンポ
    tempoStage = c - '0';
    tempoMs = 30000 / tempoBpmTable[tempoStage];

  } else if (c == 'A') {
    // 音楽開始の合図: 4声部を ROUND_OFFSET 刻みで自動的に順次開始する。
    playing = true;
    playPos = 0;
    lastStepTime = millis();

  } else if (c == 'X') {
    // 停止合図: 全サーボを基準角へ戻す
    playing = false;
    for (int i = 0; i < NUM_SERVOS; i++) {
      servos[i].write(REST_ANGLE);
      servoReturnAt[i] = 0;
    }
  }
}

// このステップで鳴っている全4声部の音階に対応するサーボを動かす
// (各声部は ROUND_OFFSET ずつずれて順次入ってくる; pos<0 の声部は moveServoForPos 内で無視)
void stepServos() {
  unsigned long releaseAt = millis() + (unsigned long)(tempoMs * 0.8); // ゲート率80%
  for (int v = 0; v < VOICE_COUNT; v++) {
    moveServoForPos(playPos - v * ROUND_OFFSET, releaseAt);
  }
}

// 指定位置の音階に対応するサーボを動かす(休符・未開始は何もしない)
void moveServoForPos(long pos, unsigned long releaseAt) {
  if (pos < 0) return;
  int idx = pitchToServoIndex(melody[pos % melodyLength]);
  if (idx >= 0) {
    servos[idx].write(ACTIVE_ANGLE);
    servoReturnAt[idx] = releaseAt;        // 発音時間が過ぎたら基準角へ戻す
  }
}

// 発音時間が過ぎたサーボを基準角へ戻す
void releaseServos() {
  unsigned long now = millis();
  for (int i = 0; i < NUM_SERVOS; i++) {
    if (servoReturnAt[i] != 0 && now >= servoReturnAt[i]) {
      servos[i].write(REST_ANGLE);
      servoReturnAt[i] = 0;
    }
  }
}

// 音程(Hz)を 0〜5 のサーボ番号へ変換(該当なしは -1 = 休符)
int pitchToServoIndex(float pitch) {
  if (pitch <= 0.0) return -1;
  for (int i = 0; i < NUM_SERVOS; i++) {
    if (fabs(pitch - NOTE_HZ[i]) < 1.0) return i;
  }
  return -1;
}
