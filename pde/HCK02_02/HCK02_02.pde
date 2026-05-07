import ddf.minim .*;
import ddf.minim.ugens .*;
import processing.serial.*;
Minim minim;
AudioOutput out;
Waveform currentWaveform ;
Serial port;
float [] mic;
int col;



String[] melody = {
  "C4", "C4", "G4", "G4", "A4", "A4", "G4","F4", "F4", "E4", "E4", "D4", "D4", "C4"
};

float[] duration = {
  1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 2.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 2.0f
};

float[] startTime = {
  0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f
};
//amplitudesの配列を追加する
float[] amplitudes = {
  0.9f, 0.3f, 0.9f, 0.3f, 0.9f, 0.3f, 1.0f, 0.9f, 0.3f, 0.9f, 0.3f, 0.9f, 0.3f, 1.0f
};

class HackInstrument implements Instrument{
 Oscil wave;
 Line ampEnv;
 float maxAmp;
 HackInstrument ( float frequency , float maxAmp , Waveform wf )
{ 
 wave = new Oscil( frequency , 0, wf );
 this.maxAmp = maxAmp;
 ampEnv = new Line();
 ampEnv.patch( wave. amplitude );
}
  
  void noteOn(float duration) {
    ampEnv.activate(duration, this.maxAmp, 0);
    wave.patch(out);
  }

  void noteOff() {
    wave.unpatch(out);
  }
}

void setup() {
  size(512, 200);
  minim = new Minim(this);
  out = minim.getLineOut();
  out.setTempo(120);
  currentWaveform = Waves.SINE;
  port = new Serial(this, "/dev/cu.usbmodem34B7DA6194E42",921600);
  port.clear();
  mic = new float[width]; 
}

void playSong() {
  out.pauseNotes();
  for (int i = 0; i < melody.length; i++) {
    out.playNote(
      startTime[i], 
      duration[i],
      new HackInstrument(Frequency.ofPitch(melody[i]).asHz(), amplitudes[i], currentWaveform)
      //new HackInstrument(Frequency.ofPitch(melody[i]).asHz(), 0.5f, currentWaveform)  この0.5fがamp
    );
  }
  
  out.resumeNotes();
}

void draw() {
  background(0);
  stroke(255);
  for (int i = 0; i < mic.length - 1; i++) {
    line(i, 100 + mic[i] * 50, i + 1, 100 + mic[i+1] * 50);
  }
}

void serialEvent(Serial p) {
  String inString = p.readStringUntil('\n');
  if (inString != null) {
    float a = float(trim(inString));
    if (!Float.isNaN(a)) {
      System.arraycopy(mic, 1, mic, 0, mic.length - 1);
      mic[mic.length - 1] = a;
    }
  }
}

void keyPressed() {
  switch(key) {
    case '1':
      currentWaveform = Waves.SINE;
      break;
    case '2':
      currentWaveform = Waves.TRIANGLE; 
      break;
    case '3':
      currentWaveform = Waves.SAW; 
      break;
    case '4':
      currentWaveform = Waves.SQUARE; 
      break;
    case 'p':
      playSong(); 
      break;
  }
}
