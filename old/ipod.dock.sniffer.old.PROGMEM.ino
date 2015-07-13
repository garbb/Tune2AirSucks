//#include <SoftwareSerial.h>                  //the following pins on this arduino mini are dead: A0, A1, D12
#include <AltSoftSerial.h>         //always uses RX=pin8, TX=pin9
#include "podserialstate.h"                  //note: try to avoid pin 13 b/c this is attached to built-in LED

#define DEBUG

//for debugging (if I want to send commands later to this will need to swap which swserial is .listen()-ing )
//SoftwareSerial DebugSerial(3, 2); // RX, TX
#define DebugSerial Serial
//AltSoftSerial DebugSerial;

//for Tx and Rx to ipod (later BT chip)
//SoftwareSerial IpodSerial(10, 11); // RX, TX    //ipod sending is pin12 on ipod cable
//SoftwareSerial IpodSerial(3, 2);
//#define IpodSerial = Serial;
//AltSoftSerial IpodSerial;          //always uses RX=pin8, TX=pin9
//SoftwareSerial IpodSerial(8, 9);  //can't use together with altsoft b/c altsoft uses these pins

//for Tx and Rx to dock
#define DockSerial Serial  //alias/reference to built-in Serial  //ipod receiving(dock sending) is pin13 on ipod cable
//SoftwareSerial DockSerial(10, 11);
//AltSoftSerial DockSerial;         //always uses RX=pin8, TX=pin9
//SoftwareSerial DockSerial(8, 9);  //can't use together with altsoft b/c altsoft uses these pins


/*
int TxToDock(byte buf[], int len) {
  return DockSerialforTx.write(buf, len);
}
int TxToIpod(byte buf[], int len) {  //(later BT chip)
  return IpodSerial.write(buf, len);
}
*/

PodserialState dockserialState(DockSerial, "dock->ipod");
//PodserialState ipodserialState(IpodSerial, "ipod->dock");


void setup() {
  DebugSerial.begin(115200);
  dockserialState.setdebugserial(DebugSerial);
//  IpodSerial.begin(19200);
//  DockSerial.listen();      //have to pick which sw serial can .read() bytes (only one at a time can)
                            //if didn't do .listen() then last one we did .begin() on will be listening

//  pinMode(3, OUTPUT);  //toggle 3.3v "ipod" ouput voltage on pin 18 for dock to sense
//  digitalWrite(3, LOW);

  delay(5000);
  DebugSerial.print("ready\r\n\r\n");
}

void loop() {
  
  dockserialState.process();
//  ipodserialState.process();

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


void PodserialState::process() {
  if (pSerial->available() <= 0) return;
  
  int b = pSerial->read();
  
  //if any state other than waiting for 1st header byte then check for header (in event of lost bytes, should never happen if serial doesn't suck)
//  if (receiveState != WAITING_FOR_HEADER1) {
//    if (maybeheader) {
//      if (b == 0x55) {
//        //dump what we have
//        for (int i = 0; i <= currentBufferpos; i++) {
//          DebugSerial.print(dataBuffer[i], HEX);
//          DebugSerial.print(",");
//        }
//        //debug out notification
//        DebugSerial.println(F("...UNEXPECTED HEADER"));
//        //now process like normal
//        receiveState = WAITING_FOR_LENGTH;
//      }
//      else {  //2nd byte was not 0x55 so go back to checking for 0xFF again
//        maybeheader = false;
//      }
//    }
//    if (b == 0xFF) {
//      maybeheader = true;
//    }
//  }
  
  switch (receiveState)
  {
  case WAITING_FOR_HEADER1:
      if (b == 0xFF) {
          receiveState = WAITING_FOR_HEADER2;
      }
//      else {
//        gotextraB = true;
//        DebugSerial.print("0x"); DebugSerial.print(b, HEX); DebugSerial.print(",");
//      }
      break;
  case WAITING_FOR_HEADER2:
      if (b == 0x55) {
          receiveState = WAITING_FOR_LENGTH;
      }
//      else {
//        gotextraB = true;
//        DebugSerial.print("0x"); DebugSerial.print(b, HEX); DebugSerial.print(",");
//      }
      break;
  case WAITING_FOR_LENGTH:
//      if (gotextraB) {
//        DebugSerial.println();
//        gotextraB = false;
//      }
      dataSize = b;
      if (dataSize == 0x00) {   //must be bogus, hopefully eventually I will get good hw serial or something that actually works
        receiveState = WAITING_FOR_HEADER1;  //want to start over for next byte
        return;
      }
      //pData = dataBuffer;
      currentBufferpos = 0;
      receiveState = WAITING_FOR_DATA;
      break;
  case WAITING_FOR_DATA:
      //if (currentBufferpos > dataBufferSize - 1) {DebugSerial.println("dataBuffer OVERFLOW");}
      
      dataBuffer[currentBufferpos] = b;      //load data into buffer
      if (currentBufferpos + 1 >= dataSize) {//if this is the last byte of data
          receiveState = WAITING_FOR_CHECKSUM;          
      }
      currentBufferpos++;

//      if (currentBufferpos + 1 <= dataSize) {
//        dataBuffer[currentBufferpos] = b;      //load byte into buffer
//        currentBufferpos++;
//      }
//      else {
//        receiveState = WAITING_FOR_CHECKSUM;  
//      }
      break;
  case WAITING_FOR_CHECKSUM:
      //EVENTUALLY CHECK CHECKSUM HERE...
      receivedchecksum = b;
      mode = dataBuffer[0];
      command[0] = dataBuffer[1];
      command[1] = dataBuffer[2];
      paramsize = dataSize - 3;  //size of params is size of response - (modebyte(1) + commandbytes(2))
      pParambytes = (dataBuffer + 3);  //should point to beginning of parameter bytes

#if defined(DEBUG)
      DebugSerial.print(millis()); DebugSerial.print(" ");    //timestamp
      DebugSerial.println(streamName);  //name of stream on 1st line
      //just print out all the bytes (if we don't know wtf they mean)
      DebugSerial.print(F("size:")); DebugSerial.print(dataSize);
      DebugSerial.print(F(" mode:")); DebugSerial.print(mode);
      DebugSerial.print(F(" command:0x")); DebugSerial.print(command[0], HEX); DebugSerial.print(",0x"); DebugSerial.print(command[1], HEX);
      DebugSerial.print(F(" params:"));
      for (int i = 0; i <= paramsize - 1; i++) {
        DebugSerial.print(pParambytes[i], HEX);
        DebugSerial.print(",");
      }
      DebugSerial.print(F(" csum:")); DebugSerial.print(receivedchecksum, HEX);
      byte expectedcsum = validChecksum();
      if (expectedcsum == receivedchecksum) {
        DebugSerial.print(F(" [OK]"));
      }
      else {
        DebugSerial.print(F(" [NG expected ")); DebugSerial.print(expectedcsum, HEX); DebugSerial.print(F("]"));
        //##################OH YEAH SHOULD PROBABLY NOT PROCESS THE COMMAND IF THE CHECKSUM IS BAD DUH###############
      }
      
      DebugSerial.println();
#endif

      //here try to print out what the bytes mean
       switch (mode) {
        case MODE_SWITCHING_MODE:
          switch (command[0]) {
            case 0x01:
              DebugSerial.print(F("switch to mode "));
              DebugSerial.println(command[1]);
              //0xFF,0x55,0x04,0x00,0x02,0x00,0x05,0xF5 mystery response from ipod that i think may be confirmation of switching to mode4??
              if (command[1] == 0x04) {
                static const byte mode4confirm[] = {0x05};
                send_response(MODE_SWITCHING_MODE, 0x02, 0x00, mode4confirm, 1);
                }
              break;
            case 0x03: //we don't care what command[1] is but it am guessing it will be 0x00?
              DebugSerial.println(F("get mode"));
              send_response(MODE_SWITCHING_MODE, 0x04, ADVANCED_REMOTE_MODE);  //response: "current mode is mode 4"
              break;
            case 0x04:
              DebugSerial.print(F("response: mode is:"));
              DebugSerial.println(command[1]);
              break;
            case 0x05:  //we don't care what command[1] is but it should be 0x00? (was 0x00 from my car dock)
              DebugSerial.println(F("switch to mode 4"));
              static const byte mode4confirm[] = {0x05};
              send_response(MODE_SWITCHING_MODE, 0x02, 0x00, mode4confirm, 1);
              break;
            case 0x06:
              DebugSerial.println(F("switch to mode 2"));
              break;
          }
          break;
        case SIMPLE_REMOTE_MODE:
          if (command[0] != 0x00) {break;}  //always supposed to be 0x00
          switch (paramsize) {
            case 0:
              switch (command[1]) {
                case 0x00: DebugSerial.println(F("button released")); break;
                case 0x01: DebugSerial.println(F("play/pause")); break;
                case 0x02: DebugSerial.println(F("vol+")); break;
                case 0x04: DebugSerial.println(F("vol-")); break;
                case 0x08: DebugSerial.println(F("skip>")); break;
                case 0x10: DebugSerial.println(F("skip<")); break;
                case 0x20: DebugSerial.println(F("next album")); break;
                case 0x40: DebugSerial.println(F("prev album")); break;
                case 0x80: DebugSerial.println(F("stop")); break;
              }
            case 1:
              switch (pParambytes[0]) {
                case 0x01: DebugSerial.println(F("play")); break;
                case 0x02: DebugSerial.println(F("pause")); break;
                case 0x04: DebugSerial.println(F("toggle mute")); break;
                case 0x20: DebugSerial.println(F("next playlist")); break;
                case 0x40: DebugSerial.println(F("prev playlist")); break;
                case 0x80: DebugSerial.println(F("toggle shuffle")); break;
              }
            case 2:
              switch (pParambytes[1]) {
                case 0x01: DebugSerial.println(F("toggle repeat")); break;
                case 0x04: DebugSerial.println(F("ipod off")); break;
                case 0x08: DebugSerial.println(F("ipod on")); break;
                case 0x40: DebugSerial.println(F("menu")); break;
                case 0x80: DebugSerial.println(F("OK/select")); break;
              }
            case 3:
              switch (pParambytes[2]) {
                case 0x01: DebugSerial.println(F("scroll up")); break;
                case 0x02: DebugSerial.println(F("scroll down")); break;
              }
          }
          break;
        case ADVANCED_REMOTE_MODE:
          if (command[0] != 0x00) {break;}  //always supposed to be 0x00
          strcpy_P(buffer, (char*)pgm_read_word(&(mode4CmdNames[command[1]]))); // print command name from mode4CmdNames array
          DebugSerial.print(buffer);
          switch (command[1]) {
            case CMD_GET_IPOD_SIZE: //this is what my ipod sends, just always send this, probably doesn't matter...
              static const byte pIpodsizedata[] = {0x01, 0x0D};
              send_response(ADVANCED_REMOTE_MODE, 0x00, RESPONSE_IPOD_SIZE, pIpodsizedata, 2);
              break;

            case CMD_GET_IPOD_NAME:
              send_response(ADVANCED_REMOTE_MODE, 0x00, RESPONSE_IPOD_NAME, "TEST ipod name");
              break;

            case CMD_GET_PIC_SIZE:  //this is what my ipod sends, just always send this, probably doesn't matter...
              static const byte pPicsizedata[] = {0x01,0x36,0x00,0xA8,0x01};
              send_response(ADVANCED_REMOTE_MODE, 0x00, RESPONSE_PIC_SIZE, pPicsizedata, 2);
              break;

            case CMD_PIC_BLOCK:  //just tell it that we got picture data ok to keep it happy
              send_feedback(CMD_PIC_BLOCK, RESULT_SUCCESS);
              break;

            case CMD_PLAYBACK_CONTROL:
              switch (pParambytes[0]) {
                case 0x01: DebugSerial.print(F("play/pause")); break;
                case 0x02: DebugSerial.print(F("stop")); break;
                case 0x03: DebugSerial.print(F("skip++")); break;
                case 0x04: DebugSerial.print(F("skip--")); break;
                case 0x05: DebugSerial.print(F("FFwd")); break;
                case 0x06: DebugSerial.print(F("Rwnd")); break;
                case 0x07: DebugSerial.print(F("StopFF/RW")); break;
              }
              send_feedback(CMD_PLAYBACK_CONTROL, RESULT_SUCCESS);
              break;
              
            case CMD_GET_TIME_AND_STATUS_INFO:
                //just dummy info for now (track length 374726ms, current elapsed time 7717ms)
              send_response(ADVANCED_REMOTE_MODE, 0x00, RESPONSE_TIME_AND_STATUS_INFO, 374726, 7717, STATE_PAUSED);
              break;

            case CMD_GET_PLAYLIST_POSITION:
              send_response(ADVANCED_REMOTE_MODE, 0x00, RESPONSE_PLAYLIST_POSITION, playlistpos);
              break;

            case CMD_SET_REPEAT_MODE:
              repeatmode = (Repeatmode)pParambytes[0];
              send_feedback(CMD_SET_REPEAT_MODE, RESULT_SUCCESS);
              break;

            case CMD_SET_SHUFFLE_MODE:
              shufflemode = (Shufflemode)pParambytes[0];
              send_feedback(CMD_SET_SHUFFLE_MODE, RESULT_SUCCESS);
              break;
              
            case CMD_GET_REPEAT_MODE:
              send_response(ADVANCED_REMOTE_MODE, 0x00, RESPONSE_REPEAT_MODE, (byte *)&repeatmode, 1);
              break;
              
            case CMD_GET_SHUFFLE_MODE:
              send_response(ADVANCED_REMOTE_MODE, 0x00, RESPONSE_SHUFFLE_MODE, (byte *)&shufflemode, 1);
              break;

            case CMD_POLLING_MODE:
              if (pParambytes[0] == 0x01)
                { pollingmode = true; }
                else { pollingmode = false; }
              send_feedback(CMD_POLLING_MODE, RESULT_SUCCESS);
              break;

            case CMD_SWITCH_TO_MAIN_LIBRARY_PLAYLIST:
              send_feedback(CMD_SWITCH_TO_MAIN_LIBRARY_PLAYLIST, RESULT_SUCCESS);
              break;
              
            case CMD_SWITCH_TO_ITEM:
              switch (pParambytes[0]) {
                case TYPE_PLAYLIST: DebugSerial.print(F("playlist")); break;
                case TYPE_ARTIST: DebugSerial.print(F("artist")); break;
                case TYPE_ALBUM: DebugSerial.print(F("album")); break;
                case TYPE_GENRE: DebugSerial.print(F("genre")); break;
                case TYPE_SONG: DebugSerial.print(F("song")); break;
                case TYPE_COMPOSER: DebugSerial.print(F("composer")); break;
              }
              DebugSerial.print(F(" #")); DebugSerial.print(endianConvert(pParambytes + 1));
              send_feedback(CMD_SWITCH_TO_ITEM, RESULT_SUCCESS);
              break;
              
            case CMD_GET_ITEM_COUNT:
              switch (pParambytes[0]) {
                case TYPE_PLAYLIST: DebugSerial.print(F("playlists")); break;
                case TYPE_ARTIST: DebugSerial.print(F("artists")); break;
                case TYPE_ALBUM: DebugSerial.print(F("albums")); break;
                case TYPE_GENRE: DebugSerial.print(F("genres")); break;
                case TYPE_SONG: DebugSerial.print(F("songs")); break;
                case TYPE_COMPOSER: DebugSerial.print(F("composers")); break;
              }
              //just dummy info; tell it that all playlists have count of 5...does this even matter????
              send_response(ADVANCED_REMOTE_MODE, 0x00, RESPONSE_ITEM_COUNT, 5);
              break;
            case CMD_GET_TITLE: CMD_GET_ARTIST: CMD_GET_ALBUM:
            DebugSerial.print(endianConvert(pParambytes));   //track number
              switch (command[1]) {
                case CMD_GET_TITLE: send_response(ADVANCED_REMOTE_MODE, 0x00, RESPONSE_TITLE, "TEST.title"); break;
                case CMD_GET_ARTIST: send_response(ADVANCED_REMOTE_MODE, 0x00, RESPONSE_ARTIST, "TEST.artist"); break;
                case CMD_GET_ALBUM: send_response(ADVANCED_REMOTE_MODE, 0x00, RESPONSE_ALBUM, "TEST.album"); break;
              }
              break;



            //param for these should be a string, display it
            case RESPONSE_TITLE: case RESPONSE_ARTIST: case RESPONSE_ALBUM: case RESPONSE_IPOD_NAME:
              DebugSerial.print("\""); DebugSerial.print((char *)pParambytes); DebugSerial.print("\"");
              break;
              
            case RESPONSE_FEEDBACK:
              //DebugSerial.print("feedback for ");
              DebugSerial.print(" for ");
              //2nd+3rd param bytes should be command that this is feedback for (and 1st byte of command should always be 0x00)
              strcpy_P(buffer, (char*)pgm_read_word(&(mode4CmdNames[pParambytes[2]])));
              DebugSerial.print(buffer);
              DebugSerial.print(" is "); DebugSerial.print(pParambytes[0]);
              DebugSerial.print("(");
              switch (pParambytes[0]) {
                case 0x00: DebugSerial.print(F("SUCCESS")); break;
                case 0x02: DebugSerial.print(F("FAILURE")); break;
                case 0x04: DebugSerial.print(F("LIMIT EXCEEDED/WRONG PARAM COUNT")); break;
                default: DebugSerial.print(F("??")); break;
              }
              DebugSerial.print(")");
              break;
              
            case RESPONSE_TIME_AND_STATUS_INFO:
              DebugSerial.print(F("track length:")); DebugSerial.print(endianConvert(pParambytes)); DebugSerial.print(F("ms "));
              DebugSerial.print(F("elapsed time:")); DebugSerial.print(endianConvert(pParambytes + 4)); DebugSerial.print(F("ms "));
              DebugSerial.print(F("status:"));
              switch (pParambytes[8]) {
                case STATE_STOPPED: DebugSerial.print(F("stopped")); break;
                case STATE_PLAYING: DebugSerial.print(F("playing")); break;
                case STATE_PAUSED: DebugSerial.print(F("paused")); break;
              }
              break;
                        
            case RESPONSE_PLAYLIST_POSITION:
              DebugSerial.print(endianConvert(pParambytes));
              break;
            case RESPONSE_POLLING_MODE:
              if (pParambytes[0] == 0x01) {DebugSerial.print(F("Track changed to #")); DebugSerial.print(endianConvert(pParambytes));}
              else {DebugSerial.print(F("elapsed time is:")); DebugSerial.print(endianConvert(pParambytes + 1));}
              break;
          }
          DebugSerial.println();
          
          break;
      }
      

      DebugSerial.println();
      
      receiveState = WAITING_FOR_HEADER1;
      memset(dataBuffer, 0, sizeof(dataBuffer));  //do we need to clear buffer?
                                                  // we should know how many bytes we got and they should be overwritten...
      break;
  }
}




/*
if (rSerial.available())
      Serial.println( char(rSerial.read()) );
      
}
*/


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
