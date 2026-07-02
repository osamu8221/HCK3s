import processing.serial.*;
import ddf.minim.*;
import ddf.minim.ugens.*;
import ddf.minim.analysis.*;
import ddf.minim.effects.*;

Serial myPort;
Minim minim;
AudioOutput out;

// --- WAV録音: 出力 out(実際に鳴っている音)をそのまま録音する ---
//   音色合成クラスには一切触れず、聞こえている音をそのまま wav 保存する。
AudioRecorder recorder;   // 録音開始時に生成する
boolean recording = false;
String recFile = "";

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
  // 録音の状態と操作ガイド (r=録音開始 / s=停止&保存)
  if (recording) { fill(255, 80, 80);   text("● REC  " + recFile, 20, 70); }
  else           { fill(180, 180, 180); text("録音:  r=開始 / s=停止&保存", 20, 70); }
  fill(255);   // 後続の text() の色を戻す
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
// ============================================================
// キー操作: r = 録音開始 / s = 停止して wav 保存
//   出力 out を Minim の AudioRecorder でそのまま録音する(音色クラスには触れない)。
//   保存先はこのスケッチのフォルダ。onkai.py / lsd.py の被検にそのまま使える。
// ============================================================
void keyPressed() {
  if (key == 'r' || key == 'R') {
    if (!recording) {
      recFile = "violin_" + recTimestamp() + ".wav";
      recorder = minim.createRecorder(out, recFile);
      recorder.beginRecord();
      recording = true;
      println("● 録音開始: " + recFile);
    } else {
      println("すでに録音中です(s で停止・保存)");
    }
  } else if (key == 's' || key == 'S') {
    if (recording && recorder != null) {
      recorder.endRecord();
      recorder.save();
      recording = false;
      println("■ 録音停止・保存: " + recFile + "  (フォルダ: " + sketchPath("") + ")");
    } else {
      println("録音していません(r で開始)");
    }
  }
}

// 録音ファイル名用のタイムスタンプ (YYYYMMDD_HHMMSS)
String recTimestamp() {
  return nf(year(),4) + nf(month(),2) + nf(day(),2) + "_"
       + nf(hour(),2) + nf(minute(),2) + nf(second(),2);
}

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
