import processing.serial.*;
import ddf.minim.*;
import ddf.minim.ugens.*;
import ddf.minim.analysis.*;
import ddf.minim.effects.*;

Serial myPort;
Minim minim;
AudioOutput out;

void setup() {
  size(800, 400);
  pixelDensity(2); // 警告対策を入れる場合はここ

  minim = new Minim(this);

  // 1. まず出力を初期化する（これより上でsetGainするとエラーになるか無視されます）
  out = minim.getLineOut(Minim.MONO, 1024);

  // 2. 必ず初期化した「後」に音量を設定する！
  out.setGain(15.0); // 思い切って「20.0」まで上げてみてください

  // --- シリアル通信の初期化 ---
  String portName = "/dev/cu.usbmodem34B7DA6194E42";
  myPort = new Serial(this, portName, 115200);
  myPort.clear();
  myPort.bufferUntil('\n');
}
void draw() {
  background(20, 25, 30);

  // --- 音の波形(時間領域)を描画 ---
  // out.mix は出力中の波形バッファ。FFTスペクトルではなく生波形を表示する。
  stroke(212, 175, 55);
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
  text("Song: Controlled by Arduino", 20, 20);
  text("Instrument: Acoustic Violin", 20, 45);
}

// 資料P.10のserialEventを使用
// --- シリアル受信イベント ---
void serialEvent(Serial p) {
  String inString = p.readStringUntil('\n');
  if (inString != null) {
    inString = trim(inString);

    // ★デバッグ用：受信した生データをProcessingの黒いコンソール画面に表示する
    println("受信データ: " + inString);

    String[] data = split(inString, ',');

    if (data.length >= 2) {
      float freq = float(data[0]);
      float durMs = float(data[1]);

      // Arduinoから指示された音程と速さ(長さ)で発音
      if (freq > 0.0 && durMs > 0) {
        float durationSec = durMs / 1000.0;
        out.playNote(0, durationSec, new AcousticViolinInstrument(freq));
        println("Played: " + freq + " Hz, Dur: " + durationSec + "s");
      }
    }
  }
}
// 楽器
class AcousticViolinInstrument implements Instrument {
  Summer mix;
  Oscil bodyTri, stringSaw, vibrato;
  Noise bowPink;
  MoogFilter frictionFilter, bodyFilter;
  ADSR env;
  
  AcousticViolinInstrument(float freq) {
    mix = new Summer();
    
    bodyTri = new Oscil(freq, 0.4f, Waves.TRIANGLE);
    stringSaw = new Oscil(freq, 0.15f, Waves.SAW);
    
    vibrato = new Oscil(5.5f, freq * 0.015f, Waves.SINE);
    vibrato.offset.setLastValue(freq); 
    vibrato.patch(bodyTri.frequency); 
    vibrato.patch(stringSaw.frequency);
    
    bowPink = new Noise(0.008f, Noise.Tint.PINK); 
    frictionFilter = new MoogFilter(4500, 0.1f, MoogFilter.Type.HP); 
    
    bodyTri.patch(mix);
    stringSaw.patch(mix);
    bowPink.patch(frictionFilter).patch(mix);
    
    bodyFilter = new MoogFilter(2500, 0.2f, MoogFilter.Type.LP);
    env = new ADSR(0.8f, 0.15f, 0.1f, 0.7f, 0.4f);
    mix.patch(bodyFilter).patch(env);
  }
  
  void noteOn(float duration) { env.noteOn(); env.patch(out); }
  void noteOff() { env.noteOff(); env.unpatchAfterRelease(out); }
}
