import processing.serial.*;
import ddf.minim.*;
import ddf.minim.ugens.*;
import ddf.minim.analysis.*;
import ddf.minim.effects.*;

Serial myPort;
Minim minim;
AudioOutput out;

// ※ ビートパターン(リズム情報)は giro.pde には持たせない。
//   Arduino(giro.ino)が保持し、「擦る長さ(ms)」をシリアルで送ってくる。
//   giro.pde は受信した指示で音色生成・再生・波形表示のみを担当する。
//   (バイオリン(violin.pde)と同じ「Arduinoから演奏指示を受信する」構成)

void setup() {
  size(800, 400);
  minim = new Minim(this);
  out = minim.getLineOut(Minim.MONO, 1024);

  // --- シリアル通信の初期化(violin.pde と同じ方式) ---
  String portName = "/dev/cu.usbmodemXXXXXXXX"; // ★ギロ用Arduinoのポートに合わせる
  myPort = new Serial(this, portName, 115200);
  myPort.clear();
  myPort.bufferUntil('\n');

  println("=== Guiro (Arduino-driven beat) ===");
}

void draw() {
  background(40, 30, 20); // ウッドブラウンの背景

  // --- 音の波形(時間領域)を描画 ---
  // out.mix の生波形をそのまま表示する。
  stroke(200, 150, 80); // ウッディなオレンジ
  strokeWeight(2);
  noFill();
  beginShape();
  for (int i = 0; i < out.bufferSize(); i++) {
    float x = map(i, 0, out.bufferSize(), 0, width);
    float y = height / 2 + out.mix.get(i) * (height / 2);
    vertex(x, y);
  }
  endShape();
  strokeWeight(1);

  fill(255);
  textSize(16);
  textAlign(LEFT, TOP);
  text("Role: Beat Rhythm Controller", 20, 20);
  text("Instrument: Wood Guiro 🪵 (Arduino-driven)", 20, 45);
}

// --- シリアル受信イベント ---
// Arduino(giro.ino)から「擦る長さ(ms)」を1行ずつ受け取って発音する。
// violin.pde の serialEvent と同じ作り(CSVの先頭フィールドを使う)。
void serialEvent(Serial p) {
  String inString = p.readStringUntil('\n');
  if (inString != null) {
    inString = trim(inString);

    // ★デバッグ用：受信した生データをコンソールに表示
    println("受信データ: " + inString);

    String[] data = split(inString, ',');

    if (data.length >= 1) {
      float durMs = float(data[0]);   // 擦る長さ(ms)

      // Arduinoから指示された長さで「ギロを擦る」音を発音
      if (durMs > 0) {
        float duration = durMs / 1000.0;
        out.playNote(0, duration, new PercussiveGuiroInstrument(duration));
        println("Beat Hit! Length: " + duration + "s");
      }
    }
  }
}

// ==========================================
// 音階を持たない純粋な打楽器としての「ギロ」
// ==========================================
class PercussiveGuiroInstrument implements Instrument {
  Summer mix;
  Oscil scrapeClicks;
  Noise faintNoise;
  MoogFilter woodResonance;
  ADSR env;

  PercussiveGuiroInstrument(float duration) {
    mix = new Summer();

    // ① 長いビートはゆっくり「ガリガリ」、短いビートは速く「シャッ」と擦る
    float scrapeSpeed = (duration < 0.1f) ? 60.0f : 25.0f;
    scrapeClicks = new Oscil(scrapeSpeed, 0.8f, Waves.SAW);

    // ② 隠し味の摩擦ノイズ
    faintNoise = new Noise(0.02f, Noise.Tint.WHITE);

    scrapeClicks.patch(mix);
    faintNoise.patch(mix);

    // ③ 硬い木管の響き（固定ピッチ：2200Hz）
    woodResonance = new MoogFilter(2200, 0.6f, MoogFilter.Type.BP);

    // ④ アタックが鋭いパーカッション用エンベロープ
    env = new ADSR(0.9f, 0.01f, 0.05f, 0.6f, 0.05f);

    mix.patch(woodResonance).patch(env);
  }

  void noteOn(float duration) {
    env.noteOn();
    env.patch(out);
  }

  void noteOff() {
    env.noteOff();
    env.unpatchAfterRelease(out);
  }
}
