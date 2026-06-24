// --- 楽器用Arduino (バイオリン担当 / カエルの歌は輪唱曲) ---
// ボード: Arduino UNO R4 WiFi (オンボードESP32-S3 / WiFiS3ライブラリ)
//
// 役割:
//   ・親機(同期制御 SyncMain)へ【TCPで自己登録】し、WELCOME→READY の後、
//     【UDPテキスト命令】 START / ROUND / LEVEL:n を受信する。
//       → ★情報伝達は Codes 2 の InstClient と同一の InstClass(Instfunc.cpp)に統一。
//   ・カエルの歌を【楽器ごとに声部を分けた輪唱】にして、この機が担当する1声部の音符を
//     "Hz,ms" で Processing(violin.pde)へ送信する。
//   ・Processing側は音色生成と再生・波形表示のみを担当する(.pdeは変更しない)。
//
// ★2026-06-24: 通信を SyncClient(UDPユニキャスト '0-4'/'A'/'X') から
//   Codes 2 の InstClass(TCP登録 + UDPテキスト START/ROUND/LEVEL)へ全置換。
//   輪唱のずれは VOICE_ID の自動遅延を廃止し、【親機主導】に変更:
//     この機は「自分宛ての START または ROUND」を受けた時点で playPos=0 から演奏を開始する。
//     どの機がいつ入るか(輪唱のずれ)は親機 SyncMain の PLAYBACK_TARGET_GROUPS が決める。
//
// 接続:
//   USB Serial (115200) -> このボード専用PCの violin.pde (バイオリンの音色・再生・波形)
//     ※「Arduino 1台 につき PC 1台」。各楽器は自分のPCでProcessingを動かす。
//   WiFi (TCP登録 + UDPテキスト命令) <- 親機 SyncMain (192.168.4.1)
//
// 親機 SyncMain から受信するUDPテキスト命令 (名前指定つき):
//   "START" / "START:inst1" / "START:inst1,inst2" : 演奏開始(自分宛てなら開始)
//   "ROUND" / "ROUND:inst2"                        : 輪唱で後から参加(自分宛てなら開始)
//   "LEVEL:1" 〜 "LEVEL:3"                          : 速度レベル(全機が受信して反映)
//
// この機の名前は config.h の myname で設定する(violin担当=inst1 など)。

#include "Instfunc.h"     // Codes 2 と同一: 情報伝達(TCP登録/UDPテキスト命令受信)
#include "config.h"       // 通信設定 + この機の myname
#include "FrogMatrix.h"   // ドットのカエル(LEDマトリクス)

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
// テンポ: 親機から来る LEVEL:n と bpm をリンクさせる
//   8分音符の長さ(ms) = 30000 / bpm
//   LEVEL 1:120bpm(250ms) 2:140bpm(214ms) 3:160bpm(188ms)
//   ※index0(100bpm)とindex4(180bpm)は将来の拡張用に残してある。
// ============================================================
const int NUM_TEMPOS = 3;
const int tempoBpmTable[NUM_TEMPOS] = {100, 140, 180};
int tempoMs = 250;          // 8分音符の長さ(ms) = 30000 / bpm (既定 120bpm)

// 演奏状態(非ブロッキングなシーケンサ)
bool playing = false;
long playPos = 0;           // 演奏開始(自分宛てSTART/ROUND)からの経過ステップ(0開始)
unsigned long lastStepTime = 0;

InstClass inst;             // Codes 2 と同一: 情報伝達
FrogMatrix frog;            // ドットのカエル

// 接続を始めるまでの遅延管理(InstClient.inoと同じ作り)
unsigned long bootAt = 0;
bool connectionReady = false;

void setup() {
  inst.Starts();            // Serial(115200)+WiFi接続+UDP開始 (-> violin.pde)
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

  // 3. マスタークロックによる再生(millis()で刻むので途中のテンポ変更に即応)
  if (playing) {
    unsigned long now = millis();
    if (now - lastStepTime >= (unsigned long)tempoMs) {
      lastStepTime = now;
      sendCurrentVoice();
      playPos++; // 次のステップへ(メロディは剰余でループ＝無限カノン)
    }
  }

  // 音が流れている間「ドットのカエル」をループさせる(再生中のみ進む)
  frog.update();
}

// この命令が自分(myname)宛てか判定する。
//   "START"            → 全楽器が対象(true)
//   "START:inst1"      → myname==inst1 の機だけ対象
//   "START:inst1,inst2"→ カンマ区切りのいずれかに一致すれば対象
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
//   親機主導: 自分宛ての START / ROUND を受けた時点で playPos=0 から演奏を開始する。
void handleCommand(String command) {
  if (command.length() == 0) return;

  if (commandTargetsMe(command, "START") || commandTargetsMe(command, "ROUND")) {
    // 自分の出番: 先頭から演奏開始(輪唱のずれは親機の送信タイミングで決まる)
    inst.ready = 2;
    startPlayback();
    Serial.println(command.startsWith("ROUND") ? "ROUND" : "START");

  } else if (command.startsWith("LEVEL:") && command.length() > 6) {
    // 速度レベル: tempoMsを即時更新 → 次ステップから新テンポ(途中変更に即応)
    int level = command.substring(6).toInt();
    inst.currentLevel = level;
    if (level < 0) level = 0;
    if (level > NUM_TEMPOS - 1) level = NUM_TEMPOS - 1;
    tempoMs = 30000 / tempoBpmTable[level];
    Serial.print("LEVEL:");
    Serial.println(inst.currentLevel);
  }
}

// 演奏を先頭から開始する(自分宛てSTART/ROUND受信時)。
void startPlayback() {
  playing = true;
  playPos = 0;
  lastStepTime = millis();
  frog.start();              // ★音と同時にLEDマトリクスのカエルを開始
}

// この機が担当する1声部の現在の音符を Processing へ送信する。
void sendCurrentVoice() {
  int durationMs = (int)(tempoMs * 0.8);   // ゲート率80%
  sendVoiceNote(playPos, durationMs);
}

// 指定位置の音符を1行 "Hz,ms" で送信(休符は送らない)。
void sendVoiceNote(long pos, int durationMs) {
  if (pos < 0) return;
  float pitch = melody[pos % melodyLength];
  if (pitch <= 0.0) return;       // 休符は送らない
  Serial.print(pitch);
  Serial.print(",");
  Serial.println(durationMs);     // 改行付きで送信(1声部=1行)
}
