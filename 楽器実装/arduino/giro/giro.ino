// --- 楽器用Arduino (ギロ担当 / リズム) ---
// ボード: Arduino UNO R4 WiFi (オンボードESP32-S3 / WiFiS3ライブラリ)
//
// 役割:
//   ・親機(同期制御 SyncMain / Codes)へ【UDP HELLO で自己登録】し、WELCOME→READY の後、
//     UDP命令 START / STOP / LEVEL:n を受信する(情報伝達は InstClass / Instfunc.cpp)。
//   ・ギロのビートパターン(リズム)を【このArduinoが保持】し、各8分音符ステップの
//     「擦る長さ(ms)」を giro.pde へシリアル送信する。
//   ・giro.pde 側は音色生成・再生・波形表示のみを担当する(.pdeは変更しない)。
//   ・「演奏開始」でオンボードLEDマトリクスの「ドットのカエル」を音と同時に開始する。
//
// 接続:
//   USB Serial (115200) -> このボード専用PCの giro.pde
//   WiFi (UDP登録 + UDP命令) <- 親機 SyncMain (192.168.4.1)
//
// ★2026-06-25: 親機を Codes 3 に更新。プロトコルが変わったため楽器側も合わせた:
//   ・開始は名前指定ではなく、親機がこの機のIP宛にunicastするプレーンな "START"。
//   ・"ROUND" は廃止(輪唱のずれは親機が8拍ごとに次グループへunicastして作る)。
//   ・"STOP" を追加(停止して開始待ちへ戻り、レベルを既定=2へ戻す)。
//   ・LEVEL→BPMは Codes 3 と統一: LEVEL 1/2/3 = 80/100/120 bpm。
//
// 親機 SyncMain(Codes 3) から受信するUDP命令 (バイオリン用inoと共通):
//   "START"   : 演奏開始(この機のIP宛にunicast。受信=自分の出番)
//   "STOP"    : 停止(broadcast)。開始待ちへ戻り LEVEL=2 に戻す
//   "LEVEL:1" 〜 "LEVEL:3" : 速度レベル(broadcast。全機が受信して反映)
//
// この機の名前は config.h の myname で設定する(ギロ担当=inst2 など)。

#include "Instfunc.h"     // Codes と同一: 情報伝達(UDP登録/UDPテキスト命令受信)
#include "config.h"       // 通信設定 + この機の myname
#include "FrogMatrix.h"   // ドットのカエル(LEDマトリクス)

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
// テンポ: 親機 Codes 3 の LEVEL_BPM と一致させる(輪唱の8拍ずれが揃うため必須)。
//   8分音符の長さ(ms) = 30000 / bpm
//   LEVEL 1:80bpm(375ms) 2:100bpm(300ms) 3:120bpm(250ms)
//   ※添字は level-1。既定は DEFAULT_LEVEL(=2, 100bpm)。
// ============================================================
const int LEVEL_BPM[3] = {70, 90, 110};   // Codes 3 SyncMain と同一
int tempoMs = 30000 / LEVEL_BPM[DEFAULT_LEVEL - 1];  // 既定 100bpm = 300ms

// 演奏状態
bool playing = false;
long stepPos = 0;           // 演奏開始からの経過ステップ(0開始, 剰余でパターンをループ)
unsigned long lastStepTime = 0;

InstClass inst;             // Codes 2 と同一: 情報伝達
FrogMatrix frog;            // ドットのカエル

// 接続を始めるまでの遅延管理(InstClient.inoと同じ作り)
unsigned long bootAt = 0;
bool connectionReady = false;

void setup() {
  inst.Starts();            // Serial(115200)+WiFi接続+UDP開始 (-> giro.pde)
  frog.begin();             // LEDマトリクス初期化(まだ消灯)
  bootAt = millis();
}

void loop() {
  // 1. まずはUDP登録(HELLO→WELCOME)、READY通知を待つ
  if (inst.ready < 1) {
    if (!connectionReady && millis() - bootAt >= INITIAL_CONNECT_DELAY_MS) {
      connectionReady = true;
    }
    if (connectionReady) inst.connection();
    return;
  }

  // 2. READY後: 親機からのUDP命令を監視(START=演奏開始 / STOP=停止 / LEVEL=テンポ)
  handleCommand(inst.recieveCommand());

  // 3. マスタークロックによるリズム再生(millis()で非ブロッキング)
  if (playing) {
    unsigned long now = millis();
    if (now - lastStepTime >= (unsigned long)tempoMs) {
      lastStepTime = now;
      sendCurrentBeat();
      stepPos++; // 次のステップへ(パターンは剰余でループ)
    }
  }

  // 音が流れている間「ドットのカエル」をループさせる(再生中のみ進む)
  frog.update();
}

// 受信したUDP命令を処理する(Codes 3: START/STOP/LEVEL)。
//   START は親機がこの機のIP宛にunicastするので、受信したら自分の出番=先頭から開始。
void handleCommand(String command) {
  if (command.length() == 0) return;

  if (command == "START") {
    inst.ready = 2;
    startPlayback();         // 先頭(stepPos=0)からリズム開始
    Serial.println("START");

  } else if (command == "STOP") {
    inst.ready = 1;          // 開始待ちへ戻す
    stopPlayback();
    inst.currentLevel = DEFAULT_LEVEL;
    tempoMs = 30000 / LEVEL_BPM[DEFAULT_LEVEL - 1];   // 既定テンポへ戻す
    Serial.println("STOP");

  } else if (command.startsWith("LEVEL:") && command.length() > 6) {
    int level = command.substring(6).toInt();
    if (level < 1) level = 1;
    if (level > 3) level = 3;
    inst.currentLevel = level;
    tempoMs = 30000 / LEVEL_BPM[level - 1];
    Serial.print("LEVEL:");
    Serial.println(level);
  }
}

// リズムを先頭から開始する(START受信時)。
void startPlayback() {
  playing = true;
  stepPos = 0;
  lastStepTime = millis();
  frog.start();              // ★音と同時にLEDマトリクスのカエルを開始
}

// リズムを停止する(STOP受信時)。
void stopPlayback() {
  playing = false;
  frog.stop();               // カエル消灯
}

// 現在ステップの「擦る長さ(ms)」を giro.pde へ送信(休符は送らない)。
//   duration[ms] = tempoMs × 係数  (元の giro.pde と同じ計算)
void sendCurrentBeat() {
  float coeff = beatPattern[stepPos % patternLength];
  if (coeff <= 0.0) return;                // 休符(ウン)は送らない
  int durationMs = (int)(tempoMs * coeff);
  Serial.println(durationMs);              // giro.pde は先頭フィールド=擦る長さ(ms)
}
