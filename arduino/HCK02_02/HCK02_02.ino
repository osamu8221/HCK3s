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
int tempoMs = 250; // タイミング: 次の音符までの待機時間

void setup() {
  // 資料P.6に倣い、シリアルポートを115200で開く
  Serial.begin(115200);
}

void loop() {
  float pitch = melody[currentIndex];
  
  // 速さ(発音時間): 休符(0.0)以外なら200ms発音する
  int durationMs = (pitch > 0.0) ? 200 : 0; 
  
  // 資料P.15の Serial.print() / println() を使って送信
  Serial.print(pitch);
  Serial.print(",");
  Serial.println(durationMs); // 改行付きで送信

  currentIndex = (currentIndex + 1) % melodyLength;

  // タイミングの制御
  delay(tempoMs);
}