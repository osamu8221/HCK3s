// --- サーボ動作テスト (6機 / カエルの歌メロディ) ---
// ボード: Arduino UNO R4 WiFi
//
// 目的:
//   本番 servo.ino の【サーボ動作を単体で確認する】スケッチ。WiFi/親機は不要。
//   ・起動時に 1機ずつ動かして配線・ピンと音の対応・可動を確認(テスト1)
//   ・その後、本番と【同じメロディ(カエルの歌)】を【同じ動作ロジック】で連続再生する。
//       - 「音が変わった時だけ」サーボを上げ下げ(同音・保持音は上げっぱなし)
//       - 角度・テンポも本番 servo.ino と同一
//     → 実際の演奏時のサーボの動き・速さ・確実さをそのまま確認できる。
//
// 操作(シリアルモニタ 115200):
//   キー '1' '2' '3' を送るとテンポを LEVEL 1/2/3 = 80/100/120 bpm に切替(本番と同じ)。
//   ※メロディは電源が入っている間ずっとループ再生する(リセットで最初から)。
//
// 配線(本番と同じ):
//   サーボ6機の信号線 = A0,A1,A2,A3,A4,A5 (アナログピンをデジタル出力として使用)
//   ★サーボの電源は外部5V電源から供給し、GNDをArduinoと共通にすること。
//     (Arduinoの5Vピンから6機を駆動すると電流不足で誤動作する)

#include <Servo.h>

// ============================================================
// 本番 servo.ino と同じ設定
// ============================================================
const int NUM_SERVOS = 6;
const int   SERVO_PIN[NUM_SERVOS]  = {A0, A1, A2, A3, A4, A5};
const char* PIN_NAME[NUM_SERVOS]   = {"A0", "A1", "A2", "A3", "A4", "A5"};
const float NOTE_HZ[NUM_SERVOS]    = {523.2, 587.3, 659.2, 698.4, 784.0, 880.0}; // ドレミファソラ
const char* NOTE_NAME[NUM_SERVOS]  = {"ド(C5)", "レ(D5)", "ミ(E5)", "ファ(F5)", "ソ(G5)", "ラ(A5)"};

// 可動角(本番 servo.ino と同一)。振り幅を小さいほど速く・確実。
const int REST_ANGLE   = 0;  // 待機角(端当て回避で少し浮かせる)
const int ACTIVE_ANGLE = 80;  // 発音時の角(振り幅40°)

// テンポ(本番 servo.ino / 親機 Codes と同一)。8分音符の長さ(ms)=30000/bpm。
//   LEVEL 1/2/3 = 80/100/120 bpm。既定は LEVEL 2(100bpm=300ms)。
const int LEVEL_BPM[3] = {80, 100, 120};
int tempoMs = 30000 / LEVEL_BPM[1];   // 既定 LEVEL 2 = 100bpm = 300ms

// 本番と同じメロディ(カエルの歌)。各要素＝8分音符、0.0=休符。
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
const int melodyLength = sizeof(melody) / sizeof(melody[0]);

Servo servos[NUM_SERVOS];
bool servoUp[NUM_SERVOS];      // 各サーボの現在状態(true=上げている)
long playPos = 0;              // メロディ上の現在ステップ(剰余でループ)
unsigned long lastStepTime = 0;

void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 2000);   // モニタ無しでも進める

  Serial.println("===== サーボ動作テスト (カエルの歌) =====");

  for (int i = 0; i < NUM_SERVOS; i++) {
    servos[i].attach(SERVO_PIN[i]);
    servos[i].write(REST_ANGLE);
    servoUp[i] = false;
  }
  delay(800);  // 初期位置に落ち着くまで待つ

  // --- テスト1: 1機ずつ動かして配線・ピンと音の対応を確認 ---
  Serial.println("[テスト1] 1機ずつ ド→レ→ミ→ファ→ソ→ラ の順に動かします");
  for (int i = 0; i < NUM_SERVOS; i++) {
    Serial.print("  サーボ"); Serial.print(i);
    Serial.print(" ("); Serial.print(PIN_NAME[i]); Serial.print(") = ");
    Serial.println(NOTE_NAME[i]);
    servos[i].write(ACTIVE_ANGLE);
    delay(350);
    servos[i].write(REST_ANGLE);
    servoUp[i] = false;
    delay(250);
  }

  // --- メロディ再生開始 ---
  Serial.println("[テスト2] カエルの歌を連続再生します(本番と同じ動作ロジック)");
  Serial.println("  キー '1'/'2'/'3' でテンポ切替 (LEVEL 80/100/120 bpm)");
  playPos = 0;
  lastStepTime = millis();
}

void loop() {
  // シリアルからテンポ変更(本番のLEVEL:1〜3に相当)
  handleSerialTempo();

  // マスタークロックによるステップ送り(millis()で非ブロッキング / 本番と同一)
  unsigned long now = millis();
  if (now - lastStepTime >= (unsigned long)tempoMs) {
    lastStepTime = now;
    stepServos();
    playPos++;   // 次のステップへ(メロディは剰余でループ=無限)
  }
}

// このステップの音に対応するサーボだけを、状態が変わった時だけ動かす。
//   本番 servo.ino の stepServos と同じ「音が変わった時だけ・同音は上げっぱなし」方式。
void stepServos() {
  bool want[NUM_SERVOS];
  for (int i = 0; i < NUM_SERVOS; i++) want[i] = false;

  // 次のステップが発音開始(onset)の休符では一旦下げ、休符を挟んで繰り返す同音も各音で打鍵を見せる。
  if (!(melody[playPos % melodyLength] <= 0.0 && melody[(playPos + 1) % melodyLength] > 0.0)) {
    int idx = soundingServoIndex(playPos);   // 休符は直前の音を保持=伸ばす音は上げたまま
    if (idx >= 0) want[idx] = true;
  }

  for (int i = 0; i < NUM_SERVOS; i++) {
    if (want[i] && !servoUp[i]) {
      servos[i].write(ACTIVE_ANGLE);
      servoUp[i] = true;
    } else if (!want[i] && servoUp[i]) {
      servos[i].write(REST_ANGLE);
      servoUp[i] = false;
    }
  }
}

// 音程(Hz)を 0〜5 のサーボ番号へ変換(該当なし・休符は -1)。
int pitchToServoIndex(float pitch) {
  if (pitch <= 0.0) return -1;
  for (int i = 0; i < NUM_SERVOS; i++) {
    if (fabs(pitch - NOTE_HZ[i]) < 1.0) return i;
  }
  return -1;
}

// 指定ステップの「今鳴っている音」のサーボ番号を返す。
//   休符(0.0)のステップは直前の非休符音まで遡る=音が伸びている間はそのサーボを保持。
//   → 連続する同音だけでなく、伸ばす音でもサーボを上げっぱなしにする(動かすのは音が変わった時だけ)。
int soundingServoIndex(long pos) {
  for (int k = 0; k < melodyLength; k++) {
    long m = ((pos - k) % melodyLength + melodyLength) % melodyLength;
    if (melody[m] > 0.0) return pitchToServoIndex(melody[m]);
  }
  return -1;
}

// シリアルで '1'/'2'/'3' を受けたらテンポを LEVEL 1/2/3 に切り替える。
void handleSerialTempo() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c >= '1' && c <= '3') {
      int level = c - '0';
      tempoMs = 30000 / LEVEL_BPM[level - 1];
      Serial.print("LEVEL:"); Serial.print(level);
      Serial.print(" (" ); Serial.print(LEVEL_BPM[level - 1]);
      Serial.print("bpm / "); Serial.print(tempoMs); Serial.println("ms/step)");
    }
  }
}
