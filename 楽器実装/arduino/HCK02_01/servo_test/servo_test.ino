// --- サーボ単発テスト (6機) ---
// ボード: Arduino UNO R4 WiFi
//
// 目的:
//   本番 servo.ino を動かす前の【ハードウェア確認用】スケッチ。
//   WiFiは使わず、setup()で1回だけ全6機を順番に動かして配線・電源・可動を確認する。
//   ・どのピン(D2〜D7)がどの音(ド・レ・ミ・ファ・ソ・ラ)に対応するかを順番に提示
//   ・最後に全機を同時に動かして電源容量(突入電流)も確認
//   ※テストは1回で終わり、その後 loop() は何もしない。
//     もう一度試したいときは USB を抜き差し(またはリセット)する。
//
// 配線(本番と同じ):
//   サーボ6機の信号線 = A0,A1,A2,A3,A4,A5 (アナログピンをデジタル出力として使用)
//   ★サーボの電源は外部5V電源から供給し、GNDをArduinoと共通にすること。
//     (Arduinoの5Vピンから6機を駆動すると電流不足で誤動作する)
//   USB Serial(115200) はテスト進行のログ表示用。

#include <Servo.h>

// 本番 servo.ino と同じ設定
const int NUM_SERVOS = 6;
const int   SERVO_PIN[NUM_SERVOS]  = {A0, A1, A2, A3, A4, A5};
const char* PIN_NAME[NUM_SERVOS]   = {"A0", "A1", "A2", "A3", "A4", "A5"};
const float NOTE_HZ[NUM_SERVOS]    = {523.2, 587.3, 659.2, 698.4, 784.0, 880.0}; // ドレミファソラ
const char* NOTE_NAME[NUM_SERVOS]  = {"ド(C5)", "レ(D5)", "ミ(E5)", "ファ(F5)", "ソ(G5)", "ラ(A5)"};

const int REST_ANGLE   = 0;   // 待機角(本番と同じ)
const int ACTIVE_ANGLE = 70;  // 発音時に動かす角(本番と同じ)

const int HOLD_MS   = 500;    // ACTIVE_ANGLEで保持する時間
const int RETURN_MS = 500;    // REST_ANGLEに戻してから次へ進むまでの待ち

Servo servos[NUM_SERVOS];

void setup() {
  Serial.begin(115200);
  // Serial接続を最大2秒だけ待つ(モニタ無しでもテストは進める)
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 2000);

  Serial.println("===== サーボ単発テスト開始 (6機) =====");

  // 全機を取り付けて待機角へ
  for (int i = 0; i < NUM_SERVOS; i++) {
    servos[i].attach(SERVO_PIN[i]);
    servos[i].write(REST_ANGLE);
  }
  delay(1000); // 初期位置に落ち着くまで待つ

  // --- テスト1: 1機ずつ順番に動かす(ピンと音の対応確認) ---
  Serial.println("[テスト1] 1機ずつ ド→レ→ミ→ファ→ソ→ラ の順に動かします");
  for (int i = 0; i < NUM_SERVOS; i++) {
    Serial.print("  サーボ"); Serial.print(i);
    Serial.print(" ("); Serial.print(PIN_NAME[i]); Serial.print(") = ");
    Serial.print(NOTE_NAME[i]); Serial.print(" / ");
    Serial.print(NOTE_HZ[i]); Serial.println("Hz");

    servos[i].write(ACTIVE_ANGLE);
    delay(HOLD_MS);
    servos[i].write(REST_ANGLE);
    delay(RETURN_MS);
  }

  // --- テスト2: 全機を同時に動かす(電源容量の確認) ---
  Serial.println("[テスト2] 全6機を同時に動かします(外部電源の容量確認)");
  for (int i = 0; i < NUM_SERVOS; i++) servos[i].write(ACTIVE_ANGLE);
  delay(1000);
  for (int i = 0; i < NUM_SERVOS; i++) servos[i].write(REST_ANGLE);
  delay(1000);

  Serial.println("===== テスト完了 =====");
  Serial.println("もう一度試すには Arduino をリセット(USB抜き差し)してください。");
}

void loop() {
  // テストは setup() で1回だけ。ここでは何もしない。
}
