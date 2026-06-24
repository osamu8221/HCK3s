// --- 楽器用Arduino (ギロ担当 / リズム) ---
// ボード: Arduino UNO R4 WiFi (オンボードESP32-S3 / WiFiS3ライブラリ)
//
// 役割:
//   ・親機(同期制御 SyncMain)へ【TCPで自己登録】し、WELCOME→READY の後、
//     【UDPテキスト命令】 START / ROUND / LEVEL:n を受信する。
//       → ★情報伝達は Codes 2 の InstClient と同一の InstClass(Instfunc.cpp)に統一。
//   ・ギロのビートパターン(リズム)を【このArduinoが保持】し、各8分音符ステップの
//     「擦る長さ(ms)」を giro.pde へシリアル送信する。
//   ・giro.pde 側は音色生成・再生・波形表示のみを担当する(.pdeは変更しない)。
//   ・「演奏開始」でオンボードLEDマトリクスの「ドットのカエル」を音と同時に開始する。
//
// ★2026-06-24: 通信を SyncClient(UDPユニキャスト '0-4'/'A'/'X') から
//   Codes 2 の InstClass(TCP登録 + UDPテキスト START/ROUND/LEVEL)へ全置換。
//   【親機主導】: この機は「自分宛ての START または ROUND」を受けた時点で
//   stepPos=0 からリズムを開始する。輪唱のずれは親機 SyncMain が決める。
//
// 接続:
//   USB Serial (115200) -> このボード専用PCの giro.pde
//   WiFi (TCP登録 + UDPテキスト命令) <- 親機 SyncMain (192.168.4.1)
//
// 親機 SyncMain から受信するUDPテキスト命令 (バイオリン用inoと共通):
//   "START" / "START:inst2" : 演奏開始(自分宛てなら開始)
//   "ROUND" / "ROUND:inst2" : 輪唱で後から参加(自分宛てなら開始)
//   "LEVEL:1" 〜 "LEVEL:3"   : 速度レベル(全機が受信して反映)
//
// この機の名前は config.h の myname で設定する(ギロ担当=inst2 など)。

#include "Instfunc.h"     // Codes 2 と同一: 情報伝達(TCP登録/UDPテキスト命令受信)
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
// テンポ: 親機から来る LEVEL:n と bpm をリンク(バイオリン用inoと共通の表)。
//   8分音符の長さ(ms) = 30000 / bpm
//   LEVEL 1:120bpm 2:140bpm 3:160bpm
// ============================================================
const int NUM_TEMPOS = 5;
const int tempoBpmTable[NUM_TEMPOS] = {100, 120, 140, 160, 180};
int tempoMs = 250;          // 8分音符の長さ(ms) = 30000 / bpm (既定 120bpm)

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
  // 1. まずはTCP接続と登録、READY通知を待つ
  if (inst.ready < 1) {
    if (!connectionReady && millis() - bootAt >= INITIAL_CONNECT_DELAY_MS) {
      connectionReady = true;
    }
    if (connectionReady) inst.connection();
    return;
  }

  // 2. READY後: 親機からのUDPテキスト命令を監視(自分宛てSTART/ROUNDで演奏開始)
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

// この命令が自分(myname)宛てか判定する。
//   "START" → 全楽器対象 / "START:inst2" → inst2だけ / カンマ区切り対応。
bool commandTargetsMe(String command, const char* eventName) {
  String event(eventName);
  if (command == event) return true;

  String prefix = event + ":";
  if (!command.startsWith(prefix)) return false;

  String targets = command.substring(prefix.length());
  int start = 0;
  while (start <= targets.length()) {
    int comma = targets.indexOf(',', start);
    int end = comma >= 0 ? comma : targets.length();
    String target = targets.substring(start, end);
    target.trim();
    if (target == myname) return true;
    if (comma < 0) break;
    start = comma + 1;
  }
  return false;
}

// 受信したUDPテキスト命令を処理する。
//   親機主導: 自分宛ての START / ROUND を受けた時点で stepPos=0 からリズム開始。
void handleCommand(String command) {
  if (command.length() == 0) return;

  if (commandTargetsMe(command, "START") || commandTargetsMe(command, "ROUND")) {
    inst.ready = 2;
    startPlayback();
    Serial.println(command.startsWith("ROUND") ? "ROUND" : "START");

  } else if (command.startsWith("LEVEL:") && command.length() > 6) {
    int level = command.substring(6).toInt();
    inst.currentLevel = level;
    if (level < 0) level = 0;
    if (level > NUM_TEMPOS - 1) level = NUM_TEMPOS - 1;
    tempoMs = 30000 / tempoBpmTable[level];
    Serial.print("LEVEL:");
    Serial.println(inst.currentLevel);
  }
}

// リズムを先頭から開始する(自分宛てSTART/ROUND受信時)。
void startPlayback() {
  playing = true;
  stepPos = 0;
  lastStepTime = millis();
  frog.start();              // ★音と同時にLEDマトリクスのカエルを開始
}

// 現在ステップの「擦る長さ(ms)」を giro.pde へ送信(休符は送らない)。
//   duration[ms] = tempoMs × 係数  (元の giro.pde と同じ計算)
void sendCurrentBeat() {
  float coeff = beatPattern[stepPos % patternLength];
  if (coeff <= 0.0) return;                // 休符(ウン)は送らない
  int durationMs = (int)(tempoMs * coeff);
  Serial.println(durationMs);              // giro.pde は先頭フィールド=擦る長さ(ms)
}
