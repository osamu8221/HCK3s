// --- Sequencer.ino ---
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
int currentIndex = 0;
int tempoMs = 250; // タイミング: 次の音符までの待機時間(8分音符相当)

void setup() {
  // Processing側の設定と合わせたボーレート(115200)
  Serial.begin(115200);
}

void loop() {
  float pitch = melody[currentIndex];
  
  // 速さ(発音時間): 休符(0.0)以外なら200ms発音する
  int durationMs = (pitch > 0.0) ? 200 : 0; 
  
  // Processingへカンマ区切りで送信
  Serial.print(pitch);
  Serial.print(",");
  Serial.println(durationMs); // 最後は改行付き

  // 次の音符へ進める（最後まで行ったら最初に戻る）
  currentIndex = (currentIndex + 1) % melodyLength;

  // 次の送信タイミングまで待機
  delay(tempoMs);
}