#define BAUD 921600 // ボーレート（速め）
#define PIN 0 // A0 アナログ出⼒
#define RESOLUTION 10 // 量⼦化10bit
void setup() {
pinMode(PIN, INPUT);
Serial.begin(BAUD);
analogReadResolution(RESOLUTION);
}
void loop() {
int d = analogRead(PIN);
float a = (float) d / (pow(2, RESOLUTION)-1)*5.0;
float maxa = 3.3/2.0; 
a = a - maxa;
float mina = -maxa; 
Serial.println(a);
}