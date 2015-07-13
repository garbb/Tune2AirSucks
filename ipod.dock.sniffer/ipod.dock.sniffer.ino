//global variables
extern String trackTitle = "";
extern String trackArtist = "";
extern String trackAlbum = "";
extern uint32_t trackLength = 0;
enum PlayingState
  {
      STATE_STOPPED = 0x00,
      STATE_PLAYING,
      STATE_PAUSED,
  };
extern PlayingState playingState = STATE_STOPPED;
extern uint32_t now = millis();

#define DEBUG
#define DEBUG_SEND    //print out to the debug serial the bytes we are sending with the send_*() functions
const bool send_responses = false;
#include "podserialstate.h"

//teensy3.1 serial pins:
//  Serial1 RX:0, TX:1
//  Serial2 RX:9, TX:10
//  Serial3 RX:7, TX:8
//  Serial is usb emulated serial

//for debugging (if I want to send commands later to this will need to swap which swserial is .listen()-ing )
#define DebugSerial Serial

//for Tx and Rx to ipod (later BT chip)
//#define IpodSerial Serial2
#define IpodSerial Serial3

//for Tx and Rx to dock
#define DockSerial Serial2  //alias/reference to built-in Serial  //ipod receiving(dock sending) is pin13 on ipod cable


PodserialState dockserialState(DockSerial, "dock->ipod");
PodserialState ipodserialState(IpodSerial, "ipod->dock");

const int polling_delay = 500;  //(ms)
int last_poll = 0;

void setup() {
  DebugSerial.begin(115200);    //if teensy usb, baud rate is ignored....
  
  dockserialState.setdebugserial(DebugSerial);
  DockSerial.begin(19200);
  
  ipodserialState.setdebugserial(DebugSerial);
  IpodSerial.begin(19200);

//  pinMode(3, OUTPUT);  //toggle 3.3v "ipod" ouput voltage on pin 18 for dock to sense
//  digitalWrite(3, LOW);

  delay(1000);    //delay is for waiting for teensy usb to become a serial device
  DebugSerial.print("ready\r\n\r\n");
}

void loop() {
  
  now = millis();
  
  dockserialState.process();
//  if (dockserialState.pollingmode && now - last_poll > polling_delay) {
//    dockserialState.send_response(ADVANCED_REMOTE_MODE, 0x00, RESPONSE_POLLING_MODE, 0x04, now - dockserialState.trackstarttime);
//    last_poll = now;
//  }
  ipodserialState.process();

//if (DebugSerial.available()) {
//  int input = DebugSerial.read();
//  if (input == '1') {
//    digitalWrite(3, HIGH); DebugSerial.println("3.3v ON");
//  }
//  else {
//    if (input == '2') digitalWrite(3, LOW); DebugSerial.println("3.3v OFF");
//  }
//}
}









/*  implement states for bytes received header, length, etc...checksum
    implement timeout (1000ms?)
      actually shouldn't need timeout?? b/c will always get another command and will eventually get bytes to get a full command,
        just need to check checksum and then reject/display error if invalid
    make enum for mode4 commands; 1st byte is always 0, make enum PLAY = 2nd byte value, etc...
    do names for commands w/same number as commands in enum: static const char *STATE_NAME[] =...
    implement debugging for unexpected bytes & bad checksum
    
    make podserialstate class with recievestate enum, buffer for bytes, datasize
    call process() with params: BYREF podserial, serial object to read from, 
*/

