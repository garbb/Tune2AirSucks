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
        myDebugSerial->print(">>got OK response\n\n");
      #endif
      responseState = OK;
      }
    else if (responsebuffer.startsWith("ER")) {
      #ifdef DEBUG
        myDebugSerial->print(">>got ERROR response\n\n");
      #endif
      responseState = ERR;
      }
    
    else if (responsebuffer.startsWith("OPEN_OK A2DP")) {
      #ifdef DEBUG
        myDebugSerial->print(">>A2DP connected\n\n");
      #endif
      SendCmd("VOLUME A2DP=15");  //need to set this every time for volume level to sound right, it won't remember it
    }
    else if (responsebuffer.startsWith("OPEN_OK AVRCP")) {
      BC127_status = AVRCP_connected;
      #ifdef DEBUG
        myDebugSerial->print(">>AVRCP connected\n\n");
      #endif
    }
    else if (responsebuffer.startsWith("Ready")) {
      BC127_status = waiting_for_AVRCP_conn;
      #ifdef DEBUG
        myDebugSerial->print(">>BC127 Boot complete\n\n");
      #endif
    }
    else if (responsebuffer.startsWith("CLOSE_OK AVRCP")) {
      BC127_status = waiting_for_AVRCP_conn;
      #ifdef DEBUG
        myDebugSerial->print(">>AVRCP disconnected\n\n");
      #endif
    }
    else if (responsebuffer.startsWith("CLOSE_OK A2DP")) {
      BC127_status = waiting_for_AVRCP_conn;
      #ifdef DEBUG
        myDebugSerial->print(">>disconnected, making discoverable\n\n");
      #endif
      SendCmd("DISCOVERABLE ON");
    }

    //Ignore all AVRCP events if there is no AVRCP connection (sometimes we randomly get AVRCP_PLAY before connection even if actual playing state is paused)
    //I don't understand if this is a bug or what but lets just ignore them.
    else if (BC127_status == AVRCP_connected) {
      
      if (responsebuffer.startsWith(AVRCP_TITLE)) {
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
  
      else if (responsebuffer.startsWith("AVRCP_STOP")) { //this never seems to happen with android apps I tested but might happen with something??
        playingState = STATE_STOPPED;
        accumTrackPlaytime += now - trackstarttime;  //add track playing time when we pause/stop?
        #ifdef DEBUG
          myDebugSerial->print(">>play state changed to "); myDebugSerial->print(playingState); myDebugSerial->print("\n\n");
        #endif
      }
      else if (responsebuffer.startsWith("AVRCP_PLAY")) {
        playingState = STATE_PLAYING;
        trackstarttime = now;
        #ifdef DEBUG
          myDebugSerial->print(">>play state changed to "); myDebugSerial->print(playingState); myDebugSerial->print("\n\n");
        #endif
      }
      else if (responsebuffer.startsWith("AVRCP_PAUSE")) {
        playingState = STATE_PAUSED;
        accumTrackPlaytime += now - trackstarttime;  //add track playing time when we pause
        #ifdef DEBUG
          myDebugSerial->print(">>play state changed to "); myDebugSerial->print(playingState); myDebugSerial->print("\n\n");
        #endif
      }

    }
//    if (prevplayingState != playingState) {allowPlayPausecmd = true;}
//    prevplayingState = playingState;
    
    responsebuffer = "";
    TOstarttime = 0;
  }

  //timeout handling
  if ( (TOstarttime > 0) && (millis() - TOstarttime) > timeoutTime ) {
    #ifdef DEBUG
      myDebugSerial->print(">>response EOL timeout"); myDebugSerial->print("\n\n");
    #endif
    responseState = ERR;
    responsebuffer = "";
    TOstarttime = 0;
  }
}



