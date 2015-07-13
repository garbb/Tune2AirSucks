#include <SoftwareSerial.h>

//for Tx and Rx to ipod
SoftwareSerial IpodSerial(10, 11); // RX, TX
//only for Rx from dock (later BT chip)
Stream& DockSerial = Serial;
//for Tx to dock (later BT chip)
SoftwareSerial DockSerialforTx(12, 13); // RX, TX


int TxToDock(byte buf[], int len) {
  return DockSerialforTx.write(buf, len);
}
int TxToIpod(byte buf[], int len) {
  return IpodSerial.write(buf, len);
}

//Stream *pDockTx = &Serial;
//Stream *pIpodTx = &mySerial;

void setup() {
  Serial.begin(19200);
  IpodSerial.begin(19200);
  DockSerialforTx.begin(19200);
}

void loop() {
  
  process(DockSerial);
  //process(IpodSerial);
}


//void doserialstuff(Stream &serial) {
//  Stream *pSerial = &serial;
//void doserialstuff(Stream *pSerial) {
  
void process(Stream &rSerial) {

if (rSerial.available())
      Serial.println( char(rSerial.read()) );
//  if (pSerial->available())
//      pSerial->println( char(pSerial->read()) );
}


/*  implement states for bytes received header, length, etc...checksum
    implement timeout (1000ms?)
      actually shouldn't need timeout?? b/c will always get another command and will eventually get bytes to get a full command,
        just need to check checksum and then reject/display error if invalid
    make enum for mode4 commands; 1st byte is always 0, make enum PLAY = 2nd byte value, etc...
    do names for commands w/same number as commands in enum: static const char *STATE_NAME[] =...
    implement debugging for unexpected bytes & bad checksum
*/
