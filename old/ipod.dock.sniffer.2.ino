#include <SoftwareSerial.h>                  //the following pins on this arduino mini are dead: A0, A1, D12
#include "podserialstate.h"

//for Tx and Rx to ipod (later BT chip)
SoftwareSerial IpodSerial(10, 11); // RX, TX

//only for Rx from dock
Stream& DockSerial = Serial;  //alias/reference to built-in Serial
//for Tx to dock
//SoftwareSerial DockSerialforTx(12, 13); // RX, TX  //note: try to avoid pin 13 b/c this is attached to built-in LED

/*
int TxToDock(byte buf[], int len) {
  return DockSerialforTx.write(buf, len);
}
int TxToIpod(byte buf[], int len) {  //(later BT chip)
  return IpodSerial.write(buf, len);
}
*/

PodserialState dockserialState(DockSerial, "dock->ipod"), ipodserialState(IpodSerial, "ipod->dock");


void setup() {
  Serial.begin(19200);    //wanted to use DockSerial.begin() here but always get an error for some reason...
  IpodSerial.begin(19200);
  Serial.print("ready\r\n\r\n");
  //DockSerialforTx.begin(19200);
}

void loop() {
  
  dockserialState.process();
  ipodserialState.process();
}



void PodserialState::process() {
  if (pSerial->available() <=0) return;
  
  int b = pSerial->read();
  switch (receiveState)
  {
  case WAITING_FOR_HEADER1:
      if (b == 0xFF) {
          receiveState = WAITING_FOR_HEADER2;
      }
      break;
  case WAITING_FOR_HEADER2:
      if (b == 0x55) {
          receiveState = WAITING_FOR_LENGTH;
      }
      break;
  case WAITING_FOR_LENGTH:
      dataSize = b;
      //pData = dataBuffer;
      currentBufferpos = 0;
      receiveState = WAITING_FOR_DATA;
      break;
  case WAITING_FOR_DATA:
      dataBuffer[currentBufferpos] = b;      //load data into buffer
      if (currentBufferpos + 1 == dataSize) {//if this is the last byte of data
          receiveState = WAITING_FOR_CHECKSUM;
      }
      currentBufferpos++;
      break;
  case WAITING_FOR_CHECKSUM:
      //EVENTUALLY CHECK CHECKSUM HERE...
      checksum = b;
      mode = dataBuffer[0];
      command[0] = dataBuffer[1];
      command[1] = dataBuffer[2];
      paramsize = dataSize - 3;  //size of params is size of response - (modebyte(1) + commandbytes(2))
      pParambytes = (dataBuffer + 3);  //should point to beginning of parameter bytes

      Serial.println(streamName);  //name of stream on 1st line
      //just print out all the bytes (if we don't know wtf they mean)
      Serial.print(F("mode:")); Serial.print(mode);
      Serial.print(F(" command:")); Serial.print((command)[0], HEX); Serial.print(","); Serial.print(command[1], HEX);
      Serial.print(F(" params:"));
      for (int i = 0; i <= paramsize - 1; i++) {
        Serial.print(pParambytes[i], HEX);
        Serial.print(",");
      }
      Serial.print(F(" csum:")); Serial.print(checksum, HEX);
      byte expectedcsum = validChecksum();
      if (expectedcsum == checksum) {
        Serial.print(F(" [OK]"));
      }
      else {
        Serial.print(F(" [NG expected ")); Serial.print(expectedcsum, HEX); Serial.print(F("]"));
      }
      
      Serial.println();
      
      //here try to print out what the bytes mean
       switch (mode) {
        case 0:
          switch (command[0]) {
            case 0x01:
              Serial.print(F("switch to mode "));
              Serial.println(command[1]);
              break;
            case 0x03:
              Serial.println(F("get mode"));
              break;
            case 0x04:
              Serial.print(F("response: mode is:"));
              Serial.println(command[1]);
              break;
            case 0x05:
              Serial.println(F("switch to mode 4"));
              break;
            case 0x06:
              Serial.println(F("switch to mode 2"));
              break;
          }
          break;
        case 2:
          if (command[0] != 0x00) {break;}  //always supposed to be 0x00
          switch (paramsize) {
            case 0:
              switch (command[1]) {
                case 0x00: Serial.println(F("button released")); break;
                case 0x01: Serial.println(F("play/pause")); break;
                case 0x02: Serial.println(F("vol+")); break;
                case 0x04: Serial.println(F("vol-")); break;
                case 0x08: Serial.println(F("skip>")); break;
                case 0x10: Serial.println(F("skip<")); break;
                case 0x20: Serial.println(F("next album")); break;
                case 0x40: Serial.println(F("prev album")); break;
                case 0x80: Serial.println(F("stop")); break;
              }
            case 1:
              switch (pParambytes[0]) {
                case 0x01: Serial.println(F("play")); break;
                case 0x02: Serial.println(F("pause")); break;
                case 0x04: Serial.println(F("toggle mute")); break;
                case 0x20: Serial.println(F("next playlist")); break;
                case 0x40: Serial.println(F("prev playlist")); break;
                case 0x80: Serial.println(F("toggle shuffle")); break;
              }
            case 2:
              switch (pParambytes[1]) {
                case 0x01: Serial.println(F("toggle repeat")); break;
                case 0x04: Serial.println(F("ipod off")); break;
                case 0x08: Serial.println(F("ipod on")); break;
                case 0x40: Serial.println(F("menu")); break;
                case 0x80: Serial.println(F("OK/select")); break;
              }
            case 3:
              switch (pParambytes[2]) {
                case 0x01: Serial.println(F("scroll up")); break;
                case 0x02: Serial.println(F("scroll down")); break;
              }
          }
          break;
        case 4:
          if (command[0] != 0x00) {break;}  //always supposed to be 0x00
          strcpy_P(buffer, (char*)pgm_read_word(&(mode4CmdNames[command[1]]))); // print command name from mode4CmdNames array
          Serial.print(buffer);
          switch (command[1]) {
            //param for these should be a string, display it
            case TITLE_RESPONSE: case ARTIST_RESPONSE: case ALBUM_RESPONSE: case IPOD_NAME_RESPONSE:
              Serial.print("\""); Serial.print((char *)pParambytes); Serial.print("\"");
              break;
            case RESPONSE_FEEDBACK:
              //Serial.print("feedback for ");
              Serial.print(" for ");
              //2nd+3rd param bytes should be command that this is feedback for (and 1st byte of command should always be 0x00)
              strcpy_P(buffer, (char*)pgm_read_word(&(mode4CmdNames[pParambytes[2]])));
              Serial.print(buffer);
              Serial.print(" is "); Serial.print(pParambytes[0]);
              Serial.print("(");
              switch (pParambytes[0]) {
                case 0x00: Serial.print(F("SUCCESS")); break;
                case 0x02: Serial.print(F("FAILURE")); break;
                case 0x04: Serial.print(F("LIMIT EXCEEDED/WRONG PARAM COUNT")); break;
                default: Serial.print(F("??")); break;
              }
              Serial.print(")");
              break;
          }
          Serial.println();
          
          break;
      }
      Serial.println();
      
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
