#define DEBUG
#define DEBUG_SEND    //print out to the debug serial the bytes we are sending with the send_*() functions
const bool send_responses = true; //not really any reason to ever not send anymore?? maybe debugging??
const bool polling_debug = false;

//global variables
extern const String default_trackTitle = "Bluetooth";
extern const String default_trackArtist = "Bluetooth";
extern const String default_trackAlbum = "Bluetooth";
extern String trackTitle = default_trackTitle;
extern String trackArtist = default_trackArtist;
extern String trackAlbum = default_trackAlbum;

extern bool trackchangeCmdPending = false;
extern bool gotnewArtist = false;
extern bool gotnewAlbum = false;
extern uint32_t albumartistWaitstart = 0;
const uint32_t albumartistTO = 500; //ms

extern bool isWaitingForNewTextRequest = false;
uint32_t newTextRequestWaitstart = 0;
const uint32_t newTextRequestWaitTO = 1000; //ms

extern uint32_t playlistpos = 50;
extern uint32_t trackLength = 3558734;  //nearly 1hr (in case we can't get this info due to older AVRCP)
extern uint32_t trackstarttime = 0;
extern uint32_t accumTrackPlaytime = 0;
enum PlayingState
  {
      STATE_STOPPED = 0x00,
      STATE_PLAYING,
      STATE_PAUSED,
  };
extern PlayingState playingState = STATE_PAUSED;
extern PlayingState prevplayingState = STATE_PAUSED;
extern bool allowPlayPausecmd = true;

extern bool waitingForMetaData = false;
extern String metaDataprevtrackTitle = default_trackTitle;
enum AvrcpReconnectstate {SEND_CLOSE, SEND_OPEN}; AvrcpReconnectstate avrcpReconnectstate = SEND_CLOSE;
extern PlayingState metaDataprevplayingState = STATE_PAUSED;
extern uint32_t metaDataWaitstart = 0;
const uint32_t metaDataWaitTO = 500; //ms
String cmdstring;

extern uint32_t now = millis();

//teensy3.1 serial pins:
//  Serial1 RX:0, TX:1
//  Serial2 RX:9, TX:10
//  Serial3 RX:7, TX:8
//  Serial is usb emulated serial

#define BC127serial Serial1 //Serial1 RX:0, TX:1

//for debugging (if I want to send commands later to this will need to swap which swserial is .listen()-ing )
#define DebugSerial Serial

//for Tx and Rx to dock
#define DockSerial Serial2  //Serial2 RX:9, TX:10

#include "myBC127.h"
extern MyBC127 myBC127(BC127serial);
#include "podserialstate.h"

PodserialState dockserialState(DockSerial, "dock->ipod");

const int polling_delay = 500;  //(ms)
uint32_t last_poll = 0;

void setup() {
  DebugSerial.begin(115200);    //if teensy usb, baud rate is ignored....
  
  BC127serial.begin(9600);
  DockSerial.begin(19200);
  
  #ifdef DEBUG
    myBC127.setdebugserial(DebugSerial);
    dockserialState.setdebugserial(DebugSerial);
  #endif

//  pinMode(3, OUTPUT);  //toggle 3.3v "ipod" ouput voltage on pin 18 for dock to sense
//  digitalWrite(3, LOW);

  //delay(1000);    //delay is for waiting for teensy usb to become a serial device
  DebugSerial.print("ready\r\n\r\n");
}

void loop() {
  
  now = millis();
  
  myBC127.readResponses();
  
  dockserialState.process();

  //if ipod polling mode is turned on
  if (dockserialState.pollingmode && now - last_poll > polling_delay) {
    dockserialState.send_response(ADVANCED_REMOTE_MODE, 0x00, RESPONSE_POLLING_MODE, 0x04, 
      playingState==STATE_PLAYING ? ((now - trackstarttime) + accumTrackPlaytime) : accumTrackPlaytime);
    last_poll = now;
  }
  
  //Once we tell the dock that the track has changed, it will request the new track info and we will respond with it.
  //We want to wait and make sure we have the title, artist, album info for the new track before we tell the dock
  //that the track has changed or we might end up sending old info.
  //Also, sometimes we only get title text (no artist or album text) and in this case we will give up waiting and just send it
  //For example we only get title text for pandora advertisements.
  if (trackchangeCmdPending) {
    if ((now - albumartistWaitstart > albumartistTO) || (gotnewArtist && gotnewAlbum)) {
      //if we have no new artist/album info just copy title text
      if (!gotnewArtist) {trackArtist = trackTitle;}
      if (!gotnewAlbum) {trackAlbum = trackTitle;}
      trackstarttime = now;  //save track start time as now
      accumTrackPlaytime = 0; //clear accum. track playtime

      //send track change event and set flag and time to begin waiting for dock to request the new text
      sendTrackChangEvent();
      isWaitingForNewTextRequest = true;
      newTextRequestWaitstart = now;
      
      gotnewArtist = false;
      gotnewAlbum = false;
      trackchangeCmdPending = false;
    }
  }

  //allow debug sending of commands...
  #ifdef DEBUG
    if (DebugSerial.available() > 0) {
      byte incomingByte = DebugSerial.read();
      //DebugSerial.write(incomingByte);
      BC127serial.write(incomingByte);
    }
  #endif

  //check if we are waiting for dock to request text and it fails to do so within timeout then resend track change event
  if (isWaitingForNewTextRequest && now - newTextRequestWaitstart > newTextRequestWaitTO) {
    #ifdef DEBUG
      DebugSerial.println(">>RESENDING sending track changed event");
    #endif
    sendTrackChangEvent();
    newTextRequestWaitstart = now;
  }

  //if playingstate changed to PLAYING then begin waiting for new metadata
  if (metaDataprevplayingState != playingState) {
    if (playingState == STATE_PLAYING) {
        waitingForMetaData = true;
        metaDataWaitstart = now;
      }
      metaDataprevplayingState = playingState;
  }
  //if we get new track change metadata before metadata timeout then don't do AVRCP reconnect below
  if ((metaDataprevtrackTitle != trackTitle)) {
    if (now - metaDataWaitstart < metaDataWaitTO) {
      waitingForMetaData = false;
    }
    metaDataprevtrackTitle = trackTitle;
  }
  else if (
      (waitingForMetaData) &&
//      (myBC127.BC127_status == myBC127.AVRCP_connected) &&
      (myBC127.AVRCP_linkId != "" && myBC127.AVRCP_address != "") &&
      (now - metaDataWaitstart > metaDataWaitTO)
     )
  {
      if (myBC127.BC127_status == myBC127.AVRCP_connected && avrcpReconnectstate == SEND_CLOSE) {
        cmdstring = "CLOSE ";
        cmdstring += myBC127.AVRCP_linkId;
        myBC127.SendCmd(cmdstring);
        avrcpReconnectstate = SEND_OPEN;
      }
      else if (myBC127.BC127_status == myBC127.waiting_for_AVRCP_conn && avrcpReconnectstate == SEND_OPEN) {
        cmdstring = "OPEN ";
        cmdstring += myBC127.AVRCP_address;
        cmdstring += " AVRCP";
        myBC127.SendCmd(cmdstring);
        avrcpReconnectstate = SEND_CLOSE;
        waitingForMetaData = false;
    }
  }

//this was for toggling 3.3v on pin18
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

void sendTrackChangEvent() {
  dockserialState.send_response(ADVANCED_REMOTE_MODE, 0x00, RESPONSE_POLLING_MODE, 0x01, playlistpos);
  #ifdef DEBUG
    DebugSerial.println(">>sending track changed event and new track number to ipod dock");
    DebugSerial.print(">>TITLE IS: \""); DebugSerial.print(trackTitle); DebugSerial.println("\"");
    DebugSerial.print(">>ARTIST IS: \""); DebugSerial.print(trackArtist); DebugSerial.println("\"");
    DebugSerial.print(">>ALBUM IS: \""); DebugSerial.print(trackAlbum); DebugSerial.println("\"");
    DebugSerial.println();
  #endif
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

