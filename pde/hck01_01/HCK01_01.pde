import ddf.minim.*;
import ddf.minim.ugens.*;

 Minim minim;
 AudioOutput out;
 
 void setup()
 {
   size(512,200);
   minim = new Minim(this);
   out =minim.getLineOut();
   out.setTempo(120);
 }
 
 void playSong(){
   out.pauseNotes();
   out.playNote(0.0f,5.0,"A4");
   out. resumeNotes ();
 }
 
void draw ()
 {
background (0);
stroke (255);
for (int i = 0; i < out. bufferSize () - 1; i++)
{
  line( i, 50 - out.left.get(i)*50 , i+1, 50 - out.left.get(i+1)*50 );
  line( i, 150 - out.right.get(i)*50 , i+1, 150 - out.right.get(i+1)*50 );
}
}

void keyPressed () {
switch (key)
 {
   case 'p':
   playSong();
 break;
 }
 }
