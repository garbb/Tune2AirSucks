#include <Arduino.h>

class MyBC127
{
  public:
    enum BC127_Status {waiting_for_boot, waiting_for_AVRCP_conn, AVRCP_connected};
    BC127_Status BC127_status;
    enum ResponseState {waiting_for_response, OK, ERR};
    ResponseState responseState;

    String responsebuffer;
    uint32_t TOstarttime;  //(time when 1st byte of reponse was received, used to detect timeout)
    const uint32_t timeoutTime = 2000; //2sec
    const String EOL = String("\n\r");  //each line of BC127 response will end in this
    Stream *_serialPort;
    #ifdef DEBUG
      Stream *myDebugSerial;
    #endif

    const String AVRCP_TITLE = "AVRCP_MEDIA TITLE: ";
    const String AVRCP_ARTIST = "AVRCP_MEDIA ARTIST: ";
    const String AVRCP_ALBUM = "AVRCP_MEDIA ALBUM: ";
    const String AVRCP_LENGTH = "AVRCP_MEDIA PLAYING_TIME(MS): ";
    
    MyBC127(Stream &sp);  //constructor
    #ifdef DEBUG
      void setdebugserial(Stream &newDebugSerial);
    #endif
    void baseSendCmd(String command);   //called by all other command sending funcs
    void SendCmd(String command);
    void MusicSendCmd(String command);

    void readResponses();
};

//constructor
MyBC127::MyBC127(Stream &sp)
{
  _serialPort = &sp;
  BC127_status = waiting_for_AVRCP_conn;    //###WILL INIT TO waiting_for_boot if I teensy ever boots before BC127
  responseState = OK;
  responsebuffer = "";
  TOstarttime = 0;
}

#ifdef DEBUG
  void MyBC127::setdebugserial(Stream &newDebugSerial) {
    myDebugSerial = &newDebugSerial;
  }
#endif

void MyBC127::baseSendCmd(String command) {
  #ifdef DEBUG
    myDebugSerial->println(); myDebugSerial->print("<<send cmd: "); myDebugSerial->println(command);
  #endif
  _serialPort->print(command);
  _serialPort->print("\r");
}

void MyBC127::SendCmd(String command) {
  if (BC127_status == waiting_for_boot) {return;}
//  #ifdef DEBUG
//    myDebugSerial->print(">>sending command: "); myDebugSerial->println(command);
//  #endif
  baseSendCmd(command);
  responseState = waiting_for_response;
}

void MyBC127::MusicSendCmd(String command) {
  //if (BC127_status != AVRCP_connected) {return;}  //preventing these commands are pointless, nothing happens if they fail anyways...
  baseSendCmd(command);
}

void MyBC127::readResponses() {
  if (_serialPort->available() > 0) {
    byte b = _serialPort->read();
    responsebuffer.concat(char(b));
    if (TOstarttime == 0) {TOstarttime = millis();}   //set beginning of timeout time
  }
   
  if (responsebuffer.endsWith(EOL)) {  //end of response line
    #ifdef DEBUG
      myDebugSerial->print(responsebuffer);
    #endif
    //all the response/event types
    if (responsebuffer.startsWith("OK")) {
      #ifdef DEBUG
        myDebugSerial->println(">>got OK response");
      #endif
      responseState = OK;
      }
    else if (responsebuffer.startsWith("ER")) {
      #ifdef DEBUG
        myDebugSerial->println(">>got ERROR response");
      #endif
      responseState = ERR;
      }
    
    else if (responsebuffer.startsWith("OPEN_OK AVRCP")) {
      BC127_status = AVRCP_connected;
      #ifdef DEBUG
        myDebugSerial->println(">>AVRCP connected");
      #endif
    }
    else if (responsebuffer.startsWith("Ready")) {
      BC127_status = waiting_for_AVRCP_conn;
      #ifdef DEBUG
        myDebugSerial->println(">>BC127 Boot complete");
      #endif
    }
    else if (responsebuffer.startsWith("CLOSE_OK AVRCP")) {
      BC127_status = waiting_for_AVRCP_conn;
      #ifdef DEBUG
        myDebugSerial->println(">>AVRCP disconnected");
      #endif
    }
    else if (responsebuffer.startsWith("CLOSE_OK A2DP")) {
      BC127_status = waiting_for_AVRCP_conn;
      #ifdef DEBUG
        myDebugSerial->println(">>disconnected, making discoverable");
      #endif
      SendCmd("DISCOVERABLE ON");
    }

    else if (responsebuffer.startsWith(AVRCP_TITLE)) {
      trackTitle = (responsebuffer.substring(AVRCP_TITLE.length())).trim();
      //SEND TRACKCHANGE COMMAND TO DOCK HERE (with current playlist position)
      //trackstarttime = now;  //save track start time as now
      trackchangeCmdPending = true;
      albumartistWaitstart = now;
      //dockserialState.send_response(ADVANCED_REMOTE_MODE, 0x00, RESPONSE_POLLING_MODE, 0x01, playlistpos);
    }
    else if (responsebuffer.startsWith(AVRCP_ARTIST)) {
      trackArtist = (responsebuffer.substring(AVRCP_ARTIST.length())).trim();
      gotnewArtist = true;
    }
    else if (responsebuffer.startsWith(AVRCP_ALBUM)) {
      trackAlbum = (responsebuffer.substring(AVRCP_ALBUM.length())).trim();
      gotnewAlbum = true;
    }
    else if (responsebuffer.startsWith(AVRCP_LENGTH)) {
      trackLength = (responsebuffer.substring(AVRCP_LENGTH.length())).toInt();
      //some apps (spotify) output track length as sec instead of ms
      //we will just assume that if we get a small number then it is in sec instead of ms
      if (trackLength < 1000) {trackLength *= 1000;}
      #ifdef DEBUG
        myDebugSerial->print(">>LENGTH IS: \""); myDebugSerial->print(trackLength); myDebugSerial->println("ms\"");
      #endif
    }

    else if (responsebuffer.startsWith("AVRCP_STOP")) {
      playingState = STATE_STOPPED;
      #ifdef DEBUG
        myDebugSerial->print(">>play state changed to "); myDebugSerial->println(playingState);
      #endif
    }
    else if (responsebuffer.startsWith("AVRCP_PLAY")) {
      playingState = STATE_PLAYING;
      trackstarttime = now;
      #ifdef DEBUG
        myDebugSerial->print(">>play state changed to "); myDebugSerial->println(playingState);
      #endif
    }
    else if (responsebuffer.startsWith("AVRCP_PAUSE")) {
      playingState = STATE_PAUSED;
      accumTrackPlaytime += now - trackstarttime;  //add track playing time when we pause
      #ifdef DEBUG
        myDebugSerial->print(">>play state changed to "); myDebugSerial->println(playingState);
      #endif
    }
//    if (prevplayingState != playingState) {allowPlayPausecmd = true;}
//    prevplayingState = playingState;
    
    responsebuffer = "";
    TOstarttime = 0;
  }

  //timeout handling
  if ( (TOstarttime > 0) && (millis() - TOstarttime) > timeoutTime ) {
    #ifdef DEBUG
      myDebugSerial->println(">>response EOL timeout");
    #endif
    responseState = ERR;
    responsebuffer = "";
    TOstarttime = 0;
  }
}



