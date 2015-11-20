/*
 ******** NEED TO SET THE FOLLOWING PERMAMENT SETTINGS FOR THE BC127 CHIP ONCE
 * SET NAME=Tune2AirSucks
 * SET ENABLE_HFP=OFF
 * SET ENABLE_PBAP=OFF
 * SET AUTOCONN=1
 * SET MAX_REC=0
 * SET CODEC=7 0 1 (to enable all codecs...not sure if this works/maters???)
 * 
 * AND THEN DO "WRITE" AND "RESET"
 */

#define DEBUG
#define DEBUG_SEND    //print out to the debug serial the bytes we are sending with the send_*() functions
const bool send_responses = true; //not really any reason to ever not send anymore?? maybe debugging??
const bool polling_debug = false;

//global variables
extern const String default_trackTitle = "Bluetooth";
extern const String default_trackArtist = "Bluetooth";
extern const String default_trackAlbum = "Bluetooth";
extern String last_trackTitle = "";
extern String last_trackArtist = "";
extern String last_trackAlbum = "";
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

//timing for switching 3.3v to ipod connector pin18 to "disconnect/reconnect"
enum CurrentMode
  {
      //WAITING_FOR_1ST_CMD,
      WAITING_FOR_MODE4,
      MODE4,
  };
extern CurrentMode currentMode = WAITING_FOR_MODE4;
uint32_t mode4waitstart = 0;                  //when we started waiting for mode4
const uint32_t mode4timeout = 1000;           //how long to wait until mode4 switch command before we do disconnect/reconnect ;1000
const uint32_t mode4timeoutBootWait = 3000;   //wait at least this long since boot before 1st disconnect/reconnect
uint32_t v_pinLastDisconnectTime = 0;         //time when 3.3v pin was last disconnected
const uint32_t v_pinDisconnectDuration = 600; //time between switching pin off and on ;600
#define v_pinStateON LOW                     //state of output pin to turn on/off voltage on 3.3v pin (will be different if using p-channel mosfet)
#define v_pinStateOFF HIGH
#define v_pin 12
int v_pinState = v_pinStateON;                //state of 3.3v pin; default to ON
int reconnects = 0;                           //current reconnect count
const int maxReconnects = 3;                  //maximum disconnect/reconnect attempts to get it to mode4


extern uint32_t playlistpos = 1;
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
const uint32_t metaDataWaitTO = 2000; //ms
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

#include "SimpleTimer.h"
SimpleTimer timer;

#include "myBC127.h"
extern MyBC127 myBC127(BC127serial);
#include "podserialstate.h"


PodserialState dockserialState(DockSerial, "dock->ipod");

const int polling_delay = 500;  //(ms)
uint32_t last_poll = 0;

void setup() {
  //Turn on built-in LED to function as power indicator
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);
  
  DebugSerial.begin(115200);    //if teensy usb, baud rate is ignored....
  
  BC127serial.begin(9600);
  DockSerial.begin(19200);
  
  #ifdef DEBUG
    myBC127.setdebugserial(DebugSerial);
    dockserialState.setdebugserial(DebugSerial);
  #endif

//pin for switching 3.3v to ipod connector pin18 to simulate "disconnect/reconnect" of ipod
pinMode(v_pin, OUTPUT);
digitalWrite(v_pin, v_pinState);

  //delay(1000);    //delay is for waiting for teensy usb to become a serial device
  DebugSerial.print("teensy ready\r\n\r\n");
}

void loop() {
  timer.run();
  
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
      //only reset playing time and increment playlistpos if this is a new track (if title, artist, or album is different)
      if ( (trackTitle != last_trackTitle) || (trackArtist != last_trackArtist) || (trackAlbum != last_trackAlbum) ) {
        trackstarttime = now;  //save track start time as now
        accumTrackPlaytime = 0; //clear accum. track playtime
        //can't tell if next track/prev track/completely different track but it will probably be next track and incrementing playlistpos looks better anyways...
        //also if title is changing from default title to a new title then do not increment playlistpos
        if (last_trackTitle != default_trackTitle && trackTitle != default_trackTitle) {
          (playlistpos < 99) ? playlistpos++ : playlistpos = 1;   //wrap around to 1 if we are at 99 and try to increment
          #ifdef DEBUG
            DebugSerial.print("increment playlistpos. new playlistpos=");DebugSerial.println(playlistpos);
          #endif
        }
      }

      //send track change event and set flag and time to begin waiting for dock to request the new text
      sendTrackChangEvent();
      isWaitingForNewTextRequest = true;
      newTextRequestWaitstart = now;
      
      gotnewArtist = false;
      gotnewAlbum = false;
      trackchangeCmdPending = false;
    }
  }

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
        #ifdef DEBUG
          DebugSerial.print(now); DebugSerial.println(" metadata wait timeout so re-connecting AVRCP profile");
        #endif
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

  #ifdef DEBUG
    if (DebugSerial.available()) {
      byte incomingByte = DebugSerial.read();
      //echo
      //DebugSerial.write(incomingByte);
      //allow debug sending of commands to BC127...
      BC127serial.write(incomingByte);
    /*
      //this was for toggling 3.3v on pin18
      if (incomingByte == '1') {
        digitalWrite(v_pin, v_pinStateON); DebugSerial.println("3.3v ON");
      }
      else {
        if (incomingByte == '2') digitalWrite(v_pin, v_pinStateOFF); DebugSerial.println("3.3v OFF");
      }
    */
    /*
      if (incomingByte == '3') {
        v_pinState = v_pinStateOFF;
        digitalWrite(v_pin, v_pinState);
        DebugSerial.println("3.3v OFF");
        v_pinLastDisconnectTime = now;
      }
    */
    }
  #endif

  if (reconnects <= maxReconnects) {
    if ( (currentMode == WAITING_FOR_MODE4) && (v_pinState == v_pinStateON) && (now > mode4timeoutBootWait) && (now - mode4waitstart > mode4timeout) ) {
      v_pinState = v_pinStateOFF;
      digitalWrite(v_pin, v_pinState);
      #ifdef DEBUG
        DebugSerial.println("3.3v OFF");
      #endif
      v_pinLastDisconnectTime = now;
    }
  }

  //3.3v has been switched off, so switch it on after v_pinDisconnectDuration time
  if (v_pinLastDisconnectTime > 0 && (now - v_pinLastDisconnectTime > v_pinDisconnectDuration)) {
    v_pinState = v_pinStateON;
    digitalWrite(v_pin, v_pinState);
    reconnects++;
    #ifdef DEBUG
      DebugSerial.println("3.3v ON");
      DebugSerial.print("reconnect #"); DebugSerial.println(reconnects);
    #endif
    v_pinLastDisconnectTime = 0;    //setting this to 0 indicates that we are currently not (disconnected and in need of reconnection)
    mode4waitstart = now;   //reset timer for waiting for mode4
  }

}

void sendTrackChangEvent() {
  dockserialState.send_response(ADVANCED_REMOTE_MODE, 0x00, RESPONSE_POLLING_MODE, 0x01, playlistpos - 1);
  #ifdef DEBUG
    DebugSerial.println(">>sending track changed event and new track number to ipod dock");
    DebugSerial.print(">>TITLE IS: \""); DebugSerial.print(trackTitle); DebugSerial.println("\"");
    DebugSerial.print(">>ARTIST IS: \""); DebugSerial.print(trackArtist); DebugSerial.println("\"");
    DebugSerial.print(">>ALBUM IS: \""); DebugSerial.print(trackAlbum); DebugSerial.println("\"");
    DebugSerial.println();
  #endif
}

void SendStatusCmd_t() {
  #ifdef DEBUG
    DebugSerial.println("timer status cmd");
  #endif
  myBC127.SendStatusCmd();
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

