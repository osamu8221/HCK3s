#ifndef SYNC_CLIENT_H
#define SYNC_CLIENT_H

// ============================================================
// SyncClient ── 楽器子機(バイオリン/ギロ)共通の「情報伝達」モジュール。
//
//   ・WiFi 接続
//   ・★自己登録(helloハートビート): 親機(同期制御)へ "HELLO:<名前>" を定期送信する。
//       親機は受信した送信元IPを記録し、その楽器へ【ユニキャスト】でコマンドを配信できる。
//       (2026-06-18 同期制御がブロードキャスト→ユニキャストに変更されたため。
//        受信専用の楽器は親機へ何も送らないとIPを知らせられない → 自己登録が必要。)
//   ・同期制御からの1バイトコマンド('0'〜'4' / 'A' / 'X')を受信し、コールバックへ渡す。
//       ※UDPの受信自体は uni/broadcast 共通。ユニキャストでも parsePacket() で受かる。
//
//   ★「情報伝達は今後これに統一」。バイオリン用ino と ギロ用ino で共通利用するため、
//     両スケッチフォルダに同一内容を置いて #include する(FrogMatrix.h と同じ運用)。
// ============================================================
#include <Arduino.h>
#include <WiFiS3.h>

// 受信した1バイトコマンドを処理するコールバックの型
typedef void (*SyncCommandHandler)(char c);

class SyncClient {
  public:
    // regIntervalMs : 自己登録(hello)の再送間隔[ms]。親機再起動やIP変化にも追従できる。
    SyncClient(unsigned long regIntervalMs = 1000)
      : handler(nullptr), lastReg(0), regInterval(regIntervalMs) {}

    // setup() で1回。WiFi接続→受信開始→直ちに1回目の自己登録。
    //   hostIp   : 親機(同期制御)のIP        recvPort : この楽器が待ち受ける受信ポート
    //   regPort  : 親機が登録helloを受けるポート  nodeName : 自分の名前("violin"/"giro"等)
    void begin(const char* ssid, const char* pass,
               IPAddress hostIp, unsigned int regPort, unsigned int recvPort,
               const char* nodeName, SyncCommandHandler cb) {
      _hostIp = hostIp;
      _regPort = regPort;
      _nodeName = nodeName;
      handler = cb;

      connectWiFi(ssid, pass);
      udp.begin(recvPort);   // ユニキャスト/ブロードキャストとも、このポート宛を受信
      sendRegister();        // 接続直後に1回登録
      lastReg = millis();
    }

    // loop() で毎回呼ぶ: コマンド受信処理 + 自己登録ハートビート。
    void update() {
      receive();
      unsigned long now = millis();
      if (now - lastReg >= regInterval) {
        lastReg = now;
        sendRegister();      // 定期的に再登録(IPマッピングを親機に保ち続ける)
      }
    }

  private:
    WiFiUDP udp;
    char buf[32];
    IPAddress _hostIp;
    unsigned int _regPort;
    const char* _nodeName;
    SyncCommandHandler handler;
    unsigned long lastReg;
    unsigned long regInterval;

    // WiFi接続(接続できるまで待つ)
    void connectWiFi(const char* ssid, const char* pass) {
      int status = WL_IDLE_STATUS;
      while (status != WL_CONNECTED) {
        status = WiFi.begin(ssid, pass);
        unsigned long t0 = millis();
        while (status != WL_CONNECTED && millis() - t0 < 8000) {
          delay(300);
          status = WiFi.status();
        }
      }
    }

    // 自己登録: 親機へ "HELLO:<名前>" をユニキャスト送信。親機が送信元IPを記録する。
    void sendRegister() {
      udp.beginPacket(_hostIp, _regPort);
      udp.print("HELLO:");
      udp.print(_nodeName);
      udp.endPacket();
    }

    // 受信した全バイトをコールバックへ流す
    void receive() {
      int packetSize = udp.parsePacket();
      if (packetSize <= 0) return;
      int n = udp.read(buf, sizeof(buf));
      for (int i = 0; i < n; i++) {
        if (handler) handler(buf[i]);
      }
    }
};

#endif // SYNC_CLIENT_H
