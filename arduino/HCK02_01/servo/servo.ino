#include <Servo.h>

Servo servo1;  // 1つ目のサーボ
Servo servo2;  // 2つ目のサーボ
Servo servo3;
void setup() {
  servo1.attach(A0); 
  servo2.attach(A1); 
  servo3.attach(A2);
}

void loop() {
  // 1. 指定した角度へ「ぱっと」動く
  servo1.write(90);  // 1つ目は一気に180度へ
  servo2.write(0);    // 2つ目は一気に0度へ
  servo3.write(0);
  
  // 2. その位置のまま少し待つ（1000ミリ秒 ＝ 1秒）
  delay(500);

  // 3. 元の角度へ「ぱっと」戻る
  servo1.write(0);    // 1つ目は一気に0度へ
  servo2.write(90);  // 2つ目は一気に180度へ
  servo3.write(0);
  // 4. その位置のまま少し待つ（1秒）
  delay(500);
  servo1.write(0);    
  servo2.write(0); 
  servo3.write(90);
  delay(500);
}