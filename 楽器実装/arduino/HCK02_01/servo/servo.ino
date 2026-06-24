// --- サーボモーター用Arduino (音階に連動して動く6機) ---
// ボード: Arduino UNO R4 WiFi (オンボードESP32-S3 / WiFiS3ライブラリ)
//
// 役割:
//   ・親機(同期制御 SyncMain)へ【TCPで自己登録】し、WELCOME→READY の後、
//     【UDPテキスト命令】 START / ROUND / LEVEL:n を受信する。
//       → ★情報伝達は Codes 2 の InstClient / GitHub Instfunc.cpp と同一の InstClass に統一。
//   ・カエルの歌の【輪唱(複数声部)】を全声部ぶんシミュレートして、各ステップで鳴っている
//     全声部の「音階」に対応するサーボを動かす。
//     (カエルの歌は ド・レ・ミ・ファ・ソ・ラ の6音 → サーボ6機に1対1で割り当て)
//
// ★2026-06-24: 通信を InstClass(TCP登録 + UDPテキスト命令)に統一。
//   violin/giro と同じく InstClient.ino の流れ(connection→ready→recieveCommand)に揃えた。
//   ただしサーボ機は「全声部の可視化機」なので、handleCommand は名前一致に関係なく
//   すべての START / ROUND に反応する(LEVELも全機受信)。
//
//   【親機主導の輪唱】:
//     ・"START" を受けた時点で 第1声を playPos=0 から開始
//     ・"ROUND" を受けるたびに 次の声部を「その時点の playPos」から追加投入
//   各声部の入りのずれは親機 SyncMain の送信タイミングがそのまま反映される。
//
// 接続:
//   WiFi (TCP登録 + UDPテキスト命令) <- 親機 SyncMain (192.168.4.1)
//   サーボ6機の信号線 = A0,A1,A2,A3,A4,A5 (アナログピンをデジタル出力として使用)
//     ※サーボの電源は外部5V電源から供給し、GNDをArduinoと共通にする。
//     ※USB Serialはデバッグ表示用(任意/115200)。このボードはProcessingには繋がない。
//
// ★前提(親機 SyncMain 側):
//   ・config.h の myname(=inst3) は親機 namechild に存在する空きスロットにすること。
//   ・親機 numchild は TCP登録する子機の総数(sens + 各inst + このservo)に合わせること。
//     合っていないと READY が来ず ready が 1 に上がらない。

#include "Instfunc.h"     // Codes 2 と同一: 情報伝達(TCP登録/UDPテキスト命令受信)
#include "config.h"       // 通信設定 + この機の myname
#include <Servo.h>

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

// 輪唱(最大4声部)を追う。各声部の入りは親機の START/ROUND の到着時刻で決まる。
const int VOICE_COUNT = 4;       // 楽器台数 = 輪唱の声部数(最大)
long playPos = 0;                // STARTからの経過ステップ(0開始)
bool voiceActive[VOICE_COUNT];   // 各声部が投入済みか
long voiceStartPos[VOICE_COUNT]; // 各声部が入った時点の playPos(声部内位置 = playPos - これ)
int activeVoices = 0;            // 投入済みの声部数

// ============================================================
// 音階 → サーボの対応 (ド・レ・ミ・ファ・ソ・ラ の6音)
//   NOTE_HZ[i] が鳴る音 → SERVO_PIN[i] のサーボが動く
// ============================================================
const int NUM_SERVOS = 6;
const float NOTE_HZ[NUM_SERVOS]  = {523.2, 587.3, 659.2, 698.4, 784.0, 880.0}; // ドレミファソラ
const int   SERVO_PIN[NUM_SERVOS] = {A0, A1, A2, A3, A4, A5};
Servo servos[NUM_SERVOS];

const int REST_ANGLE   = 0;   // 待機角
const int ACTIVE_ANGLE = 70;  // 発音時に動かす角
// 各サーボを基準角へ戻す時刻(0=既に戻し済み)。非ブロッキングで戻すために使う。
unsigned long servoReturnAt[NUM_SERVOS];

// ============================================================
// テンポ: 親機の LEVEL:n と bpm をリンク (楽器用Arduinoと同一)
//   8分音符の長さ(ms) = 30000 / bpm
// ============================================================
const int NUM_TEMPOS = 3;
const int tempoBpmTable[NUM_TEMPOS] = {100, 140, 180};
int tempoMs = 250;            // 既定 120bpm 相当

bool playing = false;
unsigned long lastStepTime = 0;

InstClass inst;               // Codes 2 と同一: 情報伝達

// 接続を始めるまでの遅延管理(InstClient.inoと同じ作り)
unsigned long bootAt = 0;
bool connectionReady = false;

void setup() {
  // ★servoはヘッドレス運用があり得るため、Starts()内の while(!Serial)(無限待ち)は使わず
  //   タイムアウト付きのSerial待ちにしてから WiFi接続・UDP開始を行う。
  Serial.begin(BAUD);         // シリアルモニタは 115200 に合わせること
  unsigned long ts = millis();
  while (!Serial && millis() - ts < 3000);
  Serial.println();
  Serial.print(myname); Serial.println(" (servo) 起動");

  for (int i = 0; i < NUM_SERVOS; i++) {
    servos[i].attach(SERVO_PIN[i]);
    servos[i].write(REST_ANGLE);
    servoReturnAt[i] = 0;
  }
  for (int v = 0; v < VOICE_COUNT; v++) {
    voiceActive[v] = false;
    voiceStartPos[v] = 0;
  }

  inst.WiFiStart();           // 親機APへ接続(接続できるまで待つ / Codes 2 と同一)
  inst.startUDP();            // UDP受信開始
  Serial.print("自分のIP: "); Serial.println(WiFi.localIP());
  bootAt = millis();
}

void loop() {
  // 1. まずはTCP接続と登録、READY通知を待つ(InstClient.inoと同じ流れ)
  if (inst.ready < 1) {
    if (!connectionReady && millis() - bootAt >= INITIAL_CONNECT_DELAY_MS) {
      connectionReady = true;
    }
    if (connectionReady) inst.connection();
    return;
  }

  // 2. READY後: 親機からのUDPテキスト命令を監視。
  //    受信した生の中身は必ず表示する(届いているか確認用)。
  String cmd = inst.recieveCommand();
  if (cmd.length() > 0) {
    Serial.print("[UDP受信] '"); Serial.print(cmd); Serial.println("'");
  }
  handleCommand(cmd);
  releaseServos();            // 発音時間が過ぎたサーボを基準角へ戻す

  // 3. マスタークロックによるステップ送り(millis()で非ブロッキング)
  if (playing) {
    unsigned long now = millis();
    if (now - lastStepTime >= (unsigned long)tempoMs) {
      lastStepTime = now;
      stepServos();
      playPos++; // 次のステップへ(メロディは剰余でループ)
    }
  }

  // ハートビート: 3秒ごとに生存とWiFi状態を表示。
  static unsigned long lastBeat = 0;
  if (millis() - lastBeat >= 3000) {
    lastBeat = millis();
    Serial.print("待機中 IP="); Serial.print(WiFi.localIP());
    Serial.print(" ready="); Serial.print(inst.ready);
    Serial.print(" playing="); Serial.print(playing ? 1 : 0);
    Serial.print(" voices="); Serial.println(activeVoices);
  }
}

// 受信したUDPテキスト命令を処理する(サーボ機は対象名を問わず全声部を可視化する)。
void handleCommand(String command) {
  if (command.length() == 0) return;

  if (command.startsWith("START")) {
    // 第1声を先頭から開始(全声部の起点をリセット)
    inst.ready = 2;
    startPlayback();
    Serial.println("START");

  } else if (command.startsWith("ROUND")) {
    // 次の声部を「その時点の playPos」から追加投入(輪唱の入り)
    addVoice();
    Serial.println("ROUND");

  } else if (command.startsWith("LEVEL:") && command.length() > 6) {
    int level = command.substring(6).toInt();
    if (level < 0) level = 0;
    if (level > NUM_TEMPOS - 1) level = NUM_TEMPOS - 1;
    tempoMs = 30000 / tempoBpmTable[level];
    Serial.print("LEVEL:");
    Serial.println(level);
  }
}

// STARTで全声部をリセットし、第1声を playPos=0 から開始する。
void startPlayback() {
  playing = true;
  playPos = 0;
  lastStepTime = millis();
  for (int v = 0; v < VOICE_COUNT; v++) {
    voiceActive[v] = false;
    voiceStartPos[v] = 0;
  }
  // 第1声を投入
  voiceActive[0] = true;
  voiceStartPos[0] = 0;
  activeVoices = 1;
}

// ROUNDで次の声部を「現在の playPos」から追加投入する。
void addVoice() {
  if (!playing) return;
  if (activeVoices >= VOICE_COUNT) return;
  voiceActive[activeVoices] = true;
  voiceStartPos[activeVoices] = playPos;  // この声部はここから先頭(=声部内位置0)
  activeVoices++;
}

// このステップで鳴っている全声部の音階に対応するサーボを動かす。
void stepServos() {
  unsigned long releaseAt = millis() + (unsigned long)(tempoMs * 0.8); // ゲート率80%
  for (int v = 0; v < VOICE_COUNT; v++) {
    if (!voiceActive[v]) continue;
    moveServoForPos(playPos - voiceStartPos[v], releaseAt);
  }
}

// 指定位置の音階に対応するサーボを動かす(休符・未開始は何もしない)。
void moveServoForPos(long pos, unsigned long releaseAt) {
  if (pos < 0) return;
  int idx = pitchToServoIndex(melody[pos % melodyLength]);
  if (idx >= 0) {
    servos[idx].write(ACTIVE_ANGLE);
    servoReturnAt[idx] = releaseAt;        // 発音時間が過ぎたら基準角へ戻す
  }
}

// 発音時間が過ぎたサーボを基準角へ戻す。
void releaseServos() {
  unsigned long now = millis();
  for (int i = 0; i < NUM_SERVOS; i++) {
    if (servoReturnAt[i] != 0 && now >= servoReturnAt[i]) {
      servos[i].write(REST_ANGLE);
      servoReturnAt[i] = 0;
    }
  }
}

// 音程(Hz)を 0〜5 のサーボ番号へ変換(該当なしは -1 = 休符)。
int pitchToServoIndex(float pitch) {
  if (pitch <= 0.0) return -1;
  for (int i = 0; i < NUM_SERVOS; i++) {
    if (fabs(pitch - NOTE_HZ[i]) < 1.0) return i;
  }
  return -1;
}
