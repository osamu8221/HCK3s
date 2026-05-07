#define BAUD 921600 
#define PIN 0 
#define RESOLUTION 10 
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