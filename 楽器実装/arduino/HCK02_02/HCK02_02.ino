// --- 楽器用Arduino (バイオリン担当 / カエルの歌は輪唱曲) ---
// ボード: Arduino UNO R4 WiFi (オンボードESP32-S3 / WiFiS3ライブラリ)
//
// 役割:
//   ・親機(同期制御 SyncMain / Codes 3)へ【TCPで自己登録】し、WELCOME→READY の後、
//     UDP命令 START / STOP / LEVEL:n を受信する(情報伝達は InstClass / Instfunc.cpp)。
//   ・カエルの歌を【楽器ごとに声部を分けた輪唱】にして、この機が担当する1声部の音符を
//     "Hz,ms" で Processing(violin.pde)へ送信する。
//   ・Processing側は音色生成と再生・波形表示のみを担当する(.pdeは変更しない)。
//
//   輪唱のずれは VOICE_ID の自動遅延を廃止し【親機主導】:この機は自分宛ての START を
//   受けた時点で playPos=0 から演奏を開始する。どの機がいつ入るか(輪唱のずれ)は
//   親機 SyncMain が PLAYBACK_TARGET_GROUPS の順に8拍間隔でunicastして決める。
//
// 接続:
//   USB Serial (115200) -> このボード専用PCの violin.pde (バイオリンの音色・再生・波形)
//     ※「Arduino 1台 につき PC 1台」。各楽器は自分のPCでProcessingを動かす。
//   WiFi (TCP登録 + UDP命令) <- 親機 SyncMain (192.168.4.1)
//
// ★2026-06-24: 親機を Codes 3 に更新。プロトコルが変わったため楽器側も合わせた:
//   ・開始は名前指定 "START:inst1" ではなく、親機が【この機のIP宛にunicast】する
//     プレーンな "START"。受け取った=自分の出番なので名前判定は不要。
//   ・"ROUND" は廃止(輪唱のずれは親機が8拍ごとに次グループへunicastして作る)。
//   ・"STOP" を追加(停止して開始待ちへ戻り、レベルを既定=2へ戻す)。
//   ・LEVEL→BPMは Codes 3 と統一: LEVEL 1/2/3 = 80/100/120 bpm。
//
// 親機 SyncMain(Codes 3) から受信するUDP命令:
//   "START"   : 演奏開始(この機のIP宛にunicast。受信=自分の出番)
//   "STOP"    : 停止(broadcast)。開始待ちへ戻り LEVEL=2 に戻す
//   "LEVEL:1" 〜 "LEVEL:3" : 速度レベル(broadcast。全機が受信して反映)
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
// テンポ: 親機 Codes 3 の LEVEL_BPM と一致させる(輪唱の8拍ずれが揃うため必須)。
//   8分音符の長さ(ms) = 30000 / bpm
//   LEVEL 1:80bpm(375ms) 2:100bpm(300ms) 3:120bpm(250ms)
//   ※添字は level-1。既定は DEFAULT_LEVEL(=2, 100bpm)。
// ============================================================
const int LEVEL_BPM[3] = {80, 100, 120};   // Codes 3 SyncMain と同一
int tempoMs = 30000 / LEVEL_BPM[DEFAULT_LEVEL - 1];  // 既定 100bpm = 300ms

// 演奏状態(非ブロッキングなシーケンサ)
bool playing = false;
long playPos = 0;           // 演奏開始(START受信)からの経過ステップ(0開始)
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

  // 2. READY後: 親機からのUDP命令を監視(START=演奏開始 / STOP=停止 / LEVEL=テンポ)
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

// 受信したUDP命令を処理する(Codes 3: START/STOP/LEVEL)。
//   START は親機がこの機のIP宛にunicastするので、受信したら自分の出番=先頭から開始。
void handleCommand(String command) {
  if (command.length() == 0) return;

  if (command == "START") {
    inst.ready = 2;
    startPlayback();         // 先頭(playPos=0)から演奏開始
    Serial.println("START");

  } else if (command == "STOP") {
    inst.ready = 1;          // 開始待ちへ戻す
    stopPlayback();
    inst.currentLevel = DEFAULT_LEVEL;
    tempoMs = 30000 / LEVEL_BPM[DEFAULT_LEVEL - 1];   // 既定テンポへ戻す
    Serial.println("STOP");

  } else if (command.startsWith("LEVEL:") && command.length() > 6) {
    // 速度レベル: tempoMsを即時更新 → 次ステップから新テンポ(途中変更に即応)
    int level = command.substring(6).toInt();
    if (level < 1) level = 1;
    if (level > 3) level = 3;
    inst.currentLevel = level;
    tempoMs = 30000 / LEVEL_BPM[level - 1];
    Serial.print("LEVEL:");
    Serial.println(level);
  }
}

// 演奏を先頭から開始する(START受信時)。
void startPlayback() {
  playing = true;
  playPos = 0;
  lastStepTime = millis();
  frog.start();              // ★音と同時にLEDマトリクスのカエルを開始
}

// 演奏を停止する(STOP受信時)。
void stopPlayback() {
  playing = false;
  frog.stop();               // カエル消灯
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
