#define DEBUG

//global variables
extern String trackTitle = "";
extern String trackArtist = "";
extern String trackAlbum = "";
extern bool trackchangeCmdPending = false;
extern bool gotnewArtist = false;
extern bool gotnewAlbum = false;
extern uint32_t albumartistWaitstart = 0;
const uint32_t albumartistTO = 500; //ms
extern uint32_t trackLength = 0;
extern uint32_t trackstarttime = 0;
extern uint32_t accumTrackPlaytime = 0;
enum PlayingState
  {
      STATE_STOPPED = 0x00,
      STATE_PLAYING,
      STATE_PAUSED,
  };
extern PlayingState playingState = STATE_STOPPED;
extern uint32_t now = millis();

#include "myBC127.h"

#define BC127serial Serial1
extern MyBC127 myBC127(BC127serial);

#define DebugSerial Serial

void setup() {
  BC127serial.begin(9600);
  DebugSerial.begin(9600);

  #ifdef DEBUG
    myBC127.setdebugserial(DebugSerial);
  #endif

  //delay(2000);    //delay is for waiting for teensy usb to become a serial device
  DebugSerial.print("ready\r\n\r\n");
}


void loop() {
  myBC127.readResponses();

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
      DebugSerial.println(">>sending new track info to ipod dock");
      //dockserialState.send_response(ADVANCED_REMOTE_MODE, 0x00, RESPONSE_POLLING_MODE, 0x01, playlistpos);
      #ifdef DEBUG
        DebugSerial.print(">>TITLE IS: \""); DebugSerial.print(trackTitle); DebugSerial.println("\"");
        DebugSerial.print(">>ARTIST IS: \""); DebugSerial.print(trackArtist); DebugSerial.println("\"");
        DebugSerial.print(">>ALBUM IS: \""); DebugSerial.print(trackAlbum); DebugSerial.println("\"");
      #endif
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

}



