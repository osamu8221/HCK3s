#ifndef FROG_MATRIX_H
#define FROG_MATRIX_H

// ============================================================
// FrogMatrix ── UNO R4 WiFi オンボード 12x8 LEDマトリクスに
//   「ドットのカエル」を 3コマ でアニメーションさせる共通モジュール。
//
//   ・begin()  : setup() で1回。マトリクス初期化。
//   ・start()  : 「再生の合図」で呼ぶ。音と同時に先頭コマからアニメ開始。
//   ・update() : loop() で毎回呼ぶ。一定間隔で 3コマ をループ(再生中のみ動く)。
//   ・stop()   : 停止合図で呼ぶ。マトリクスを消灯。
//
//   ★バイオリン用ino と ギロ用ino の「2つのinoに共通」のシステム。
//     両方のスケッチフォルダに同一内容を置いて #include する。
// ============================================================
#include <Arduino.h>
#include "Arduino_LED_Matrix.h"

class FrogMatrix {
  public:
    // frameIntervalMs : 1コマあたりの表示時間[ms](既定180ms ≒ 3コマで約0.54秒の跳ね)
    FrogMatrix(unsigned long frameIntervalMs = 180)
      : interval(frameIntervalMs), frameIdx(0), active(false), lastSwitch(0) {}

    void begin() {
      matrix.begin();
      clear();
    }

    // 「再生の合図」で呼ぶ: 音と同時にアニメ開始(必ず先頭コマから)
    void start() {
      active = true;
      frameIdx = 0;
      lastSwitch = millis();
      render(frameIdx);
    }

    // 音が流れている間 loop() で呼び続ける: 3コマを順にループ
    void update() {
      if (!active) return;
      unsigned long now = millis();
      if (now - lastSwitch >= interval) {
        lastSwitch = now;
        frameIdx = (frameIdx + 1) % FRAME_COUNT;
        render(frameIdx);
      }
    }

    // 停止: 消灯してアニメを止める
    void stop() {
      active = false;
      clear();
    }

    bool isActive() const { return active; }

  private:
    static const int FRAME_COUNT = 3;  // スクリーンに映す絵の数 = 3つ
    ArduinoLEDMatrix matrix;
    unsigned long interval;
    int frameIdx;
    bool active;
    unsigned long lastSwitch;

    void render(int idx) {
      matrix.renderBitmap(FROG[idx], 8, 12);
    }

    void clear() {
      uint8_t blank[8][12];
      memset(blank, 0, sizeof(blank));
      matrix.renderBitmap(blank, 8, 12);
    }

    // 3コマのカエル(8行 × 12列)。1=点灯, 0=消灯。
    //   frame0:おすわり  frame1:沈み込み(ちぢこまり)  frame2:ジャンプ(足を伸ばす)
    static uint8_t FROG[FRAME_COUNT][8][12];
};

uint8_t FrogMatrix::FROG[3][8][12] = {
  { // frame0 ── おすわり (目を上げて座る)
    {0,0,1,0,0,0,0,0,0,1,0,0},
    {0,1,1,1,0,0,0,0,1,1,1,0},
    {0,0,1,0,0,0,0,0,0,1,0,0},
    {0,1,1,1,1,1,1,1,1,1,1,0},
    {1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1},
    {0,1,1,1,1,1,1,1,1,1,1,0},
    {1,1,0,0,0,0,0,0,0,0,1,1},
  },
  { // frame1 ── 沈み込み (体を縮める/しゃがむ)
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,1,0,0,0,0,0,0,1,0,0},
    {0,1,1,1,0,0,0,0,1,1,1,0},
    {0,0,1,1,1,1,1,1,1,1,0,0},
    {0,1,1,1,1,1,1,1,1,1,1,0},
    {1,1,1,1,1,1,1,1,1,1,1,1},
    {0,0,1,1,1,1,1,1,1,1,0,0},
    {0,1,1,0,0,0,0,0,0,1,1,0},
  },
  { // frame2 ── ジャンプ (足を左右に伸ばす)
    {0,0,1,0,0,0,0,0,0,1,0,0},
    {0,1,1,1,0,0,0,0,1,1,1,0},
    {0,0,1,0,0,0,0,0,0,1,0,0},
    {0,0,1,1,1,1,1,1,1,1,0,0},
    {0,1,1,1,1,1,1,1,1,1,1,0},
    {1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,0,1,1,1,1,1,1,0,1,1},
    {1,0,0,0,0,0,0,0,0,0,0,1},
  },
};

#endif // FROG_MATRIX_H
