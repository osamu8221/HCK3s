// --- 楽器用Arduino (バイオリン担当 / カエルの歌は輪唱曲) ---
// ボード: Arduino UNO R4 WiFi (オンボードESP32-S3 / WiFiS3ライブラリ)
//
// 役割:
//   ・同期用Arduinoから【WiFi UDPブロードキャスト】で「再生の合図」と「テンポ情報(0〜4)」を受信
//   ・カエルの歌を【楽器ごとに声部を分けた輪唱】にして、
//     このArduinoが担当する1声部の音符を "Hz,ms" で Processing へ送信
//   ・Processing側は音色生成と再生・波形表示のみを担当する
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
//   WiFi (UDP) <- 同期用Arduinoのブロードキャスト (全受信機が同一データを受信)
//
// 同期用Arduino から受信するUDPペイロード:
//   '0'〜'4' : テンポ情報。ino配列のbpmにリンクさせ、再生途中でも即時反映する。
//   'A'      : 音楽開始の合図。各機は自分の VOICE_ID に応じた遅延後に再生を始める。
//   'X'      : 停止合図。

#include <WiFiS3.h>

// ============================================================
// WiFi設定: 同期Arduinoと全受信機を同じネットワーク(同一サブネット)に入れる。
//   同期Arduinoは 255.255.255.255:SYNC_UDP_PORT にコマンドをブロードキャストする。
// ============================================================
const char* WIFI_SSID = "YOUR_SSID";       // ★各自の環境に合わせる
const char* WIFI_PASS = "YOUR_PASSWORD";   // ★各自の環境に合わせる
const unsigned int SYNC_UDP_PORT = 50007;  // 同期Arduinoと一致させる

WiFiUDP udp;
char udpBuf[32];

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

void setup() {
  Serial.begin(115200);     // -> このボード専用PCのProcessing

  connectWiFi();            // <- 同期Arduinoと同じネットワークに接続
  udp.begin(SYNC_UDP_PORT); // 同期ブロードキャストの受信開始
}

void loop() {
  // 同期Arduinoからの「テンポ変更」「開始/停止合図」を常時監視する
  handleSyncCommand();

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

// 同期Arduinoからのコマンド受信(UDPブロードキャスト)
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
    // テンポ情報(0〜4): ino配列のbpmにリンク。tempoMsを即時更新するので、
    // 次のステップから新しいテンポで再生され、途中変更が成立する。
    tempoStage = c - '0';
    tempoMs = 30000 / tempoBpmTable[tempoStage];

  } else if (c == 'A') {
    // 音楽開始の合図: 全機が同時にカウント開始。各機は VOICE_ID 分だけ自動待機して入る。
    playing = true;
    playPos = 0;
    lastStepTime = millis();

  } else if (c == 'X') {
    // 停止合図
    playing = false;
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
