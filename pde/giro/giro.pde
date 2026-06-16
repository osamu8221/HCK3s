import ddf.minim.*;
import ddf.minim.ugens.*;
import ddf.minim.analysis.*;
import ddf.minim.effects.*;

Minim minim;
AudioOutput out;
FFT fft;

// ギロ専用のビートパターン（1要素＝8分音符 250ms）
// 数値は「擦る長さ（係数）」を表します。
// 0.8 = 長く擦る(チー)、0.3 = 短く擦る(チッ)、0.0 = 休符(ウン)
float[] beatPattern = {
  // 1小節目：チー・チッ・チー・チッ (表拍を強調)
  0.8, 0.3, 0.8, 0.3, 0.8, 0.3, 0.8, 0.3,
  // 2小節目：チー・チッ・チー・チッ
  0.8, 0.3, 0.8, 0.3, 0.8, 0.3, 0.8, 0.3,
  // 3小節目：チー・ウン・チッ・チッ (少しリズムを変える)
  0.8, 0.0, 0.3, 0.3, 0.8, 0.3, 0.8, 0.3,
  // 4小節目：チッ・チッ・チッ・チッ (フィルイン：次の小節への繋ぎの連打)
  0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3
};

int currentIndex = 0;
int lastPlayTime = 0;
int tempoMs = 250; // 8分音符(半拍)の長さ

void setup() {
  size(800, 400); 
  minim = new Minim(this);
  out = minim.getLineOut(Minim.MONO, 1024);
  fft = new FFT(out.bufferSize(), out.sampleRate());
  println("=== Step 2: Guiro Beat Sequencer ===");
}

void draw() {
  background(40, 30, 20); // ウッドブラウンの背景
  int now = millis();
  
  if (now - lastPlayTime > tempoMs) {
    float beatLength = beatPattern[currentIndex];
    
    if (beatLength > 0.0) {
      // 係数から実際の再生時間(秒)を計算
      float duration = (tempoMs / 1000.0f) * beatLength;
      out.playNote(0, duration, new PercussiveGuiroInstrument(duration));
      println("Beat Hit! Length: " + duration + "s");
    } else {
      println("Beat Rest");
    }
    
    // パターンをループ
    currentIndex = (currentIndex + 1) % beatPattern.length;
    lastPlayTime = now;
  }

  // --- FFTスペクトル描画 ---
  fft.forward(out.mix);
  stroke(200, 150, 80); // ウッディなオレンジ
  fill(200, 150, 80, 150);
  
  for(int i = 0; i < fft.specSize(); i++) {
    float bandAmplitude = fft.getBand(i) * 15; 
    float x = map(i, 0, fft.specSize(), 0, width * 2); 
    if (x < width) { 
      rect(x, height, 2, -bandAmplitude);
    }
  }
  
  fill(255);
  textSize(16);
  textAlign(LEFT, TOP);
  text("Role: Beat Rhythm Controller", 20, 20);
  text("Instrument: Wood Guiro 🪵 (8-Beat Sequencer)", 20, 45);
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
