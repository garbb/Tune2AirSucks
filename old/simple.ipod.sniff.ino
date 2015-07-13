//the following pins on this arduino mini are dead: A0, A1, D12

//#include <SoftwareSerial.h>
#include <AltSoftSerial.h>

//SoftwareSerial mySerial(3, 2); // RX, TX
AltSoftSerial mySerial;

void setup() {
  Serial.begin(19200);
  mySerial.begin(19200);
  Serial.print("ready\r\n\r\n");
}

void loop() // run over and over
{
  if (mySerial.available())
    Serial.println(mySerial.read(), HEX);
    
//  if (Serial.available())
//    Serial.println(Serial.read());
}
