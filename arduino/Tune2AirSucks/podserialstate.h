#include "Arduino.h"

//on arduino int is 16-bit(2 byte) and long is 32-bit(4 byte)
//on teensy int is 32bit(4 byte)???
//so replaced all "unsigned long" with uint32_t

const byte MODE_SWITCHING_MODE = 0x00;
const byte SIMPLE_REMOTE_MODE = 0x02;
const byte ADVANCED_REMOTE_MODE = 0x04;

const byte RESPONSE_BAD = 0x00;
const byte RESPONSE_FEEDBACK = 0x01;

//these are commands
//do -1 to make response consts
const byte REQUEST_PROTOCOL_VERSION = 0x12;
const byte RETURN_PROTOCOL_VERSION = 0x13;
const byte CMD_GET_IPOD_NAME = 0x14;
const byte RESPONSE_IPOD_NAME = 0x15;
const byte CMD_SWITCH_TO_MAIN_LIBRARY_PLAYLIST = 0x16;
const byte CMD_SWITCH_TO_ITEM = 0x17;
const byte CMD_GET_ITEM_COUNT = 0x18;
const byte RESPONSE_ITEM_COUNT = 0x19;
const byte CMD_GET_ITEM_NAMES = 0x1A;
const byte RESPONSE_ITEM_NAMES = 0x1B;
const byte CMD_GET_TIME_AND_STATUS_INFO = 0x1C;
const byte RESPONSE_TIME_AND_STATUS_INFO = 0x1D;
const byte CMD_GET_PLAYLIST_POSITION = 0x1E;
const byte RESPONSE_PLAYLIST_POSITION = 0x1F;
const byte CMD_GET_TITLE = 0x20;
const byte RESPONSE_TITLE = 0x21;
const byte CMD_GET_ARTIST = 0x22;
const byte RESPONSE_ARTIST = 0x23;
const byte CMD_GET_ALBUM = 0x24;
const byte RESPONSE_ALBUM = 0x25;
const byte CMD_POLLING_MODE = 0x26;
const byte RESPONSE_POLLING_MODE = 0x27;
const byte CMD_EXECUTE_SWITCH = 0x28;
const byte CMD_PLAYBACK_CONTROL = 0x29;
const byte CMD_GET_SHUFFLE_MODE = 0x2C;
const byte RESPONSE_SHUFFLE_MODE = 0x2D;
const byte CMD_SET_SHUFFLE_MODE = 0x2E;
const byte CMD_GET_REPEAT_MODE = 0x2F;
const byte RESPONSE_REPEAT_MODE = 0x30;
const byte CMD_SET_REPEAT_MODE = 0x31;
const byte CMD_PIC_BLOCK = 0x32;
const byte CMD_GET_PIC_SIZE = 0x33;
const byte RESPONSE_PIC_SIZE = 0x34;
const byte CMD_GET_SONG_COUNT_IN_CURRENT_PLAYLIST = 0x35;
const byte RESPONSE_SONG_COUNT_IN_CURRENT_PLAYLIST = 0x36;
const byte CMD_JUMP_TO_SONG_IN_CURRENT_PLAYLIST = 0x37;

enum itemType
  {
      TYPE_PLAYLIST = 0x01,
      TYPE_ARTIST,
      TYPE_ALBUM,
      TYPE_GENRE,
      TYPE_SONG,
      TYPE_COMPOSER
  };
  
enum feedbackResult
  {
      RESULT_SUCCESS = 0x00,
      RESULT_FAILURE = 0x02,
      RESULT_LIMIT_EXCEEDED = 0x04,
      RESULT_SEND_RESPONSE = 0x04,
  };

#if defined(DEBUG)
  const char* const mode4CmdNames[0x3A] = {
    /*0x00*/ "BAD command",
    /*0x01*/ "FEEDBACK",
    /*0x02*/ "ping?",
    /*0x03*/ "pong?",
    /*0x04*/ "??",
    /*0x05*/ "??",
    /*0x06*/ "??",
    /*0x07*/ "??",
    /*0x08*/ "??",
    /*0x09*/ "??",
    /*0x0A*/ "??",
    /*0x0B*/ "??",
    /*0x0C*/ "??",
    /*0x0D*/ "??",
    /*0x0E*/ "??",
    /*0x0F*/ "??",
    /*0x10*/ "??",
    /*0x11*/ "??",
    /*0x12*/ "RequestProtocolVersion",
    /*0x13*/ "ReturnProtocolVersion",
    /*0x14*/ "get ipod name",
    /*0x15*/ "ipod name response:",
    /*0x16*/ "switch to main library",
    /*0x17*/ "switch to ",
    /*0x18*/ "get count of ",
    /*0x19*/ "count response",
    /*0x1A*/ "get names of ",
    /*0x1B*/ "names response",
    /*0x1C*/ "get time/status",
    /*0x1D*/ "time/status response: ",
    /*0x1E*/ "get plist pos",
    /*0x1F*/ "playlist position is ",
    /*0x20*/ "get title of song #",
    /*0x21*/ "title response:",
    /*0x22*/ "get artist of song #",
    /*0x23*/ "artist response:",
    /*0x24*/ "get album of song #",
    /*0x25*/ "album response:",
    /*0x26*/ "polling start/stop",
    /*0x27*/ "polling response",
    /*0x28*/ "switch to plist",
    /*0x29*/ "playback control cmd ",
    /*0x2A*/ "??",
    /*0x2B*/ "??",
    /*0x2C*/ "get shuffle mode",
    /*0x2D*/ "shuffle mode response",
    /*0x2E*/ "set shuffle mode",
    /*0x2F*/ "get repeat mode",
    /*0x30*/ "repeat mode response",
    /*0x31*/ "set repeat mode",
    /*0x32*/ "picture block",
    /*0x33*/ "get pic size?",
    /*0x34*/ "pic size response?",
    /*0x35*/ "get # of songs",
    /*0x36*/ "# of songs response",
    /*0x37*/ "jump to song #",
    /*0x38*/ "??",
    /*0x39*/ "??"
  };
#endif

//char buffer[30];  //buffer needed to read strings out of program flash memory

class PodserialState {
  public:

  uint32_t currentBufferpos;
  static const uint32_t dataBufferSize = 2048;  //hopefully big enough for packets with 2byte data size??
  byte dataBuffer[dataBufferSize];
  uint32_t dataSize;
  byte receivedchecksum;
  byte checksum;
  byte mode;
  byte command[2];
  uint32_t paramsize;
  byte *pParambytes;
  bool gotextraB;
  bool maybeheader;
  char const *streamName;
  bool pollingmode = false;
  int start_num_to_get = 0;
  int num_of_items_to_get = 0;

  Stream *pSerial;
  Stream *myDebugSerial;

  enum ReceiveState
      {
          WAITING_FOR_HEADER1 = 0,
          WAITING_FOR_HEADER2,
          WAITING_FOR_LENGTH0,
          WAITING_FOR_LENGTH1,
          WAITING_FOR_LENGTH2,
          WAITING_FOR_DATA,
          WAITING_FOR_CHECKSUM
      };
  ReceiveState receiveState;
  
  enum Shufflemode
    {
        SHUFFLE_OFF = 0x00,
        SHUFFLE_SONGS = 0x01,
        SHUFFLE_ALBUMS = 0x02,
    };
  Shufflemode shufflemode = SHUFFLE_OFF;

  enum Repeatmode
    {
        REPEAT_OFF = 0x00,
        REPEAT_ONE_SONGS = 0x01,
        REPEAT_ALL_SONGS = 0x02,
    };
  Repeatmode repeatmode = REPEAT_OFF;

  PodserialState(Stream &newiPodSerial, char const *newstreamName);

  void process();
  void setdebugserial(Stream &newDebugSerial);
  
  byte validChecksum();
  uint32_t endianConvert(const byte *p);

  void send_feedback(byte cmd_this_is_feedback_for, byte result);

  void send_response(byte mode, byte cmdbyte1, byte cmdbyte2);
  void send_response(byte mode, byte cmdbyte1, byte cmdbyte2, const byte *pData, byte paramlength);
  void send_response(byte mode, byte cmdbyte1, byte cmdbyte2, uint32_t num1, uint32_t num2, byte statusbyte);
  void send_response(byte mode, byte cmdbyte1, byte cmdbyte2, uint32_t num1);
  void send_response(byte mode, byte cmdbyte1, byte cmdbyte2, byte pollingbyte, uint32_t num1);
  void send_response(byte mode, byte cmdbyte1, byte cmdbyte2, String string);
  void send_response(byte mode, byte cmdbyte1, byte cmdbyte2, uint32_t num1, String string);

  void sendHeader();
  void sendNumber(uint32_t n);
  void sendChecksum();
  void sendByte(byte b);
};


  //constructor
    PodserialState::PodserialState(Stream &newiPodSerial, char const *newstreamName)
    :currentBufferpos(0), dataSize(0), gotextraB(false), maybeheader(false),
    streamName(newstreamName),
    pSerial(&newiPodSerial),
    receiveState(WAITING_FOR_HEADER1)
    {
    }
    
  void PodserialState::setdebugserial(Stream &newDebugSerial) {
    myDebugSerial = &newDebugSerial;
  }
   
  byte PodserialState::validChecksum()
  {
//      int expected = dataSize;
      int expected = ((dataSize >> (8)) & 0xff) + ((dataSize >> (0)) & 0xff); //two lowest bytes (for 2byte lengths)
      for (uint32_t i = 0; i < dataSize; ++i)
      {
          expected += dataBuffer[i];
      }
      expected = (0x100 - expected) & 0xFF;
      return expected;
  }
  
  //for converting form ipod/dock data received => arduino int
  uint32_t PodserialState::endianConvert(const byte *p)
  {
    return
        (((uint32_t) p[3]) << 0)  |
        (((uint32_t) p[2]) << 8)  |
        (((uint32_t) p[1]) << 16) |
        (((uint32_t) p[0]) << 24);
  }

//feedback for commands
  void PodserialState::send_feedback(byte cmd_this_is_feedback_for, byte result) {
    byte pData[] = {result, 0x00, cmd_this_is_feedback_for};
    #if defined(DEBUG_SEND)
      myDebugSerial->println();
      myDebugSerial->print("send feedback for ");
      myDebugSerial->print( mode4CmdNames[cmd_this_is_feedback_for] );
    #endif
    send_response(ADVANCED_REMOTE_MODE, 0x00, RESPONSE_FEEDBACK, pData, 3);
  }

  //for sending response with no params
  //so far only one I found like this is "get mode" response
  void PodserialState::send_response(byte mode, byte cmdbyte1, byte cmdbyte2) {
    if (!send_responses) {return;}
    #if defined(DEBUG_SEND)
      myDebugSerial->println();
      myDebugSerial->print("send ");
      myDebugSerial->print("mode:"); myDebugSerial->print(mode); myDebugSerial->print(", ");
    #endif
    sendHeader();
    sendByte(1 + 2);      //length (1 mode byte + 2 cmd bytes)
    sendByte(mode);
    sendByte(cmdbyte1);
    sendByte(cmdbyte2);
    sendChecksum();
  }

  //buffer of bytes as param
  //also used to send 0xFF,0x55,0x04,0x00,0x02,0x00,0x05,0xF5 mystery response from ipod that i think may be confirmation of switching to mode4??
  void PodserialState::send_response(byte mode, byte cmdbyte1, byte cmdbyte2, const byte *pData, byte paramlength) {
    if (!send_responses) {return;}
    #if defined(DEBUG_SEND)
      myDebugSerial->println();
      myDebugSerial->print("send ");
      myDebugSerial->print("mode:"); myDebugSerial->print(mode); myDebugSerial->print(", ");
      if (mode == ADVANCED_REMOTE_MODE){
        myDebugSerial->print( mode4CmdNames[cmdbyte2] );
      }
      else {
        myDebugSerial->print( cmdbyte1, HEX );
        myDebugSerial->print( ',' );
        myDebugSerial->print( cmdbyte2, HEX );
      }
    #endif
    sendHeader();
    sendByte(1 + 2 + paramlength);      //length (1 mode byte + 2 cmd bytes + length of param bytes)
    sendByte(mode);
    sendByte(cmdbyte1);
    sendByte(cmdbyte2);
    for (byte i = 0; i < paramlength; i++) {
      sendByte(pData[i]);
    }
    sendChecksum();
  }

  //2 numbers and one byte as param
  //mostly for time/status response
  void PodserialState::send_response(byte mode, byte cmdbyte1, byte cmdbyte2, uint32_t num1, uint32_t num2, byte statusbyte) {
    if (!send_responses) {return;}
    #if defined(DEBUG_SEND)
      myDebugSerial->println();
      myDebugSerial->print("send ");
      myDebugSerial->print("mode:"); myDebugSerial->print(mode); myDebugSerial->print(", ");
      myDebugSerial->print( mode4CmdNames[cmdbyte2] );
      myDebugSerial->print(", "); myDebugSerial->print(num1);
      myDebugSerial->print(", "); myDebugSerial->print(num2);
      myDebugSerial->print(", "); myDebugSerial->print(statusbyte);
    #endif
    sendHeader();
    sendByte(1 + 2 + 4 + 4 + 1);      //length (1 mode byte + 2 cmd bytes + 4byte int + 4byte int + byte)
    sendByte(mode);
    sendByte(cmdbyte1);
    sendByte(cmdbyte2);
    sendNumber(num1);
    sendNumber(num2);
    sendByte(statusbyte);
    sendChecksum();
  }

  //1 number as param
  //(get count, get playlist pos...)
  void PodserialState::send_response(byte mode, byte cmdbyte1, byte cmdbyte2, uint32_t num1) {
    if (!send_responses) {return;}
    #if defined(DEBUG_SEND)
      myDebugSerial->println();
      myDebugSerial->print("send ");
      myDebugSerial->print("mode:"); myDebugSerial->print(mode); myDebugSerial->print(", ");
      myDebugSerial->print( mode4CmdNames[cmdbyte2] );
      myDebugSerial->print(", "); myDebugSerial->print(num1);
    #endif
    sendHeader();
    sendByte(1 + 2 + 4);      //length (1 mode byte + 2 cmd bytes + 4byte int)
    sendByte(mode);
    sendByte(cmdbyte1);
    sendByte(cmdbyte2);
    sendNumber(num1);
    sendChecksum();
  }

  //one byte and one number (polling/track has changed message...)
  void PodserialState::send_response(byte mode, byte cmdbyte1, byte cmdbyte2, byte pollingbyte, uint32_t num1) {
    #if defined(DEBUG_SEND)
      if (pollingbyte != 0x04 || polling_debug) {    //turn off spamming debug with polling
        myDebugSerial->println();
        myDebugSerial->print("send ");
        myDebugSerial->print("mode:"); myDebugSerial->print(mode); myDebugSerial->print(", ");
        myDebugSerial->print( mode4CmdNames[cmdbyte2] );
        if (pollingbyte == 0x01) { myDebugSerial->print(" track changed to:"); }
        myDebugSerial->print(", "); myDebugSerial->print(pollingbyte);
        myDebugSerial->print(", "); myDebugSerial->print(num1);
        myDebugSerial->println();
      }
    #endif
    sendHeader();
    sendByte(1 + 2 + 1 + 4);      //length (1 mode byte + 2 cmd bytes + 1 byte + 4byte int)
    sendByte(mode);
    sendByte(cmdbyte1);
    sendByte(cmdbyte2);
    sendByte(pollingbyte);
    sendNumber(num1);
    sendChecksum();
  }

  //string as param
  //(title/artist/album/ipod name...)
  void PodserialState::send_response(byte mode, byte cmdbyte1, byte cmdbyte2, String string) {
    if (!send_responses) {return;}
    #if defined(DEBUG_SEND)
      myDebugSerial->println();
      myDebugSerial->print("send ");
      myDebugSerial->print("mode:"); myDebugSerial->print(mode); myDebugSerial->print(", ");
      myDebugSerial->print( mode4CmdNames[cmdbyte2] );
      myDebugSerial->print(", "); myDebugSerial->print(string);
    #endif

    //pre-compute checksum
    byte checksum_temp = 0;
    byte stringlength = string.length() + 1;
    checksum_temp += 1 + 2 + stringlength;
    checksum_temp += mode;
    checksum_temp += cmdbyte1;
    checksum_temp += cmdbyte2;
    for (int i=0; i<stringlength; i++) {
      checksum_temp += string[i];
    }
    checksum_temp = (0x100 - checksum_temp) & 0xFF;
    //now check if string contains checksum, if so, add leading space and hope new csum is not in string...
    //this is workaround for what I believe is a bug in my dock in which it sees checksum as end of packet?
    for (int i=0; i<stringlength; i++) {
      if (string[i] == checksum_temp) {
        string = " " + string;
        stringlength += 1;
        break;
      }
    }

    //now do actual sending of data
    sendHeader();
    sendByte(1 + 2 + stringlength);      //length (1 mode byte + 2 cmd bytes + string length)
    sendByte(mode);
    sendByte(cmdbyte1);
    sendByte(cmdbyte2);
    
    for (int i=0; i<stringlength; i++) {
      sendByte(string[i]);
    }    
    sendChecksum();
  }

  //one number and string
  //for get item names responses
  void PodserialState::send_response(byte mode, byte cmdbyte1, byte cmdbyte2, uint32_t num1, String string) {
    if (!send_responses) {return;}
    #if defined(DEBUG_SEND)
      myDebugSerial->println();
      myDebugSerial->print("send ");
      myDebugSerial->print("mode:"); myDebugSerial->print(mode); myDebugSerial->print(", ");
      myDebugSerial->print( mode4CmdNames[cmdbyte2] );
      myDebugSerial->print(", "); myDebugSerial->print(num1);
      myDebugSerial->print(", "); myDebugSerial->print(string);
    #endif
    sendHeader();
    const byte stringlength = string.length() + 1;
    sendByte(1 + 2 + 4 + stringlength);      //length (1 mode byte + 2 cmd bytes + 4byte int + string length)
    sendByte(mode);
    sendByte(cmdbyte1);
    sendByte(cmdbyte2);
    sendNumber(num1);
    char stringbuffer[stringlength];
    string.toCharArray(stringbuffer, stringlength);
    for (int i=0; i<stringlength; i++) {
      sendByte(stringbuffer[i]);
    }    
    sendChecksum();
  }
  
  void PodserialState::sendNumber(uint32_t n)
  {
      // parameter (4-byte int sent big-endian)
      sendByte((n & 0xFF000000) >> 24);
      sendByte((n & 0x00FF0000) >> 16);
      sendByte((n & 0x0000FF00) >> 8);
      sendByte((n & 0x000000FF) >> 0);
  }

  void PodserialState::sendHeader()
  {
//    #if defined(DEBUG_SEND)
//      myDebugSerial->println();
//      myDebugSerial->print("send header,");
//    #endif
    pSerial->write(0xFF);
    pSerial->write(0x55);
  }
  
  void PodserialState::sendByte(byte b)
  {
//      #if defined(DEBUG_SEND)
//        myDebugSerial->print(b, HEX);
//        myDebugSerial->print(",");
//      #endif
      pSerial->write(b);
      checksum += b;
  }

  void PodserialState::sendChecksum()
  {
      sendByte((0x100 - checksum) & 0xFF);
      //checksum = 0x00;  //need to clear checksum or it will have data for next send??
//      #if defined(DEBUG_SEND)
//        myDebugSerial->println();
//      #endif
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
//          myDebugSerial->print(dataBuffer[i], HEX);
//          myDebugSerial->print(",");
//        }
//        //debug out notification
//        myDebugSerial->println("...UNEXPECTED HEADER");
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
//        myDebugSerial->print("0x"); myDebugSerial->print(b, HEX); myDebugSerial->print(",");
//      }
      break;
  case WAITING_FOR_HEADER2:
      if (b == 0x55) {
          receiveState = WAITING_FOR_LENGTH0;
      }
//      else {
//        gotextraB = true;
//        myDebugSerial->print("0x"); myDebugSerial->print(b, HEX); myDebugSerial->print(",");
//      }
      break;
      
  case WAITING_FOR_LENGTH0:
    dataSize = b;
    if (dataSize == 0x00) {
      //IT APPEARS that sometimes (usually for picture blocks) the size will be 3bytes!! (or really next two bytes after 0x00)
      //if 1st size byte is zero then take next two bytes as size
      receiveState = WAITING_FOR_LENGTH1;
    }
    else {
      currentBufferpos = 0;
      receiveState = WAITING_FOR_DATA;
    }
    break;
    
  case WAITING_FOR_LENGTH1:
    dataSize = (((uint32_t) b) << 8); //load higher-order byte
    receiveState = WAITING_FOR_LENGTH2;
    break;
    
  case WAITING_FOR_LENGTH2:
    dataSize = dataSize | (((uint32_t) b) << 0); //load lower-order byte
    currentBufferpos = 0;
    receiveState = WAITING_FOR_DATA;
    break;

  case WAITING_FOR_DATA:
      //if (currentBufferpos > dataBufferSize - 1) {myDebugSerial->println("dataBuffer OVERFLOW");}
      
      dataBuffer[currentBufferpos] = b;      //load data into buffer
      if (currentBufferpos + 1 >= dataSize) {//if this is the last byte of data
          receiveState = WAITING_FOR_CHECKSUM;          
      }
      currentBufferpos++;
      break;
      
  case WAITING_FOR_CHECKSUM:
      receivedchecksum = b;
      mode = dataBuffer[0];
      command[0] = dataBuffer[1];
      command[1] = dataBuffer[2];
      paramsize = dataSize>3 ? dataSize - 3 : 0;  //size of params is size of response - (modebyte(1) + commandbytes(2))
      pParambytes = (dataBuffer + 3);  //should point to beginning of parameter bytes

      #if defined(DEBUG)
        myDebugSerial->print(millis()); myDebugSerial->print(" ");    //timestamp
        myDebugSerial->println(streamName);  //name of stream on 1st line
        //just print out all the bytes (if we don't know wtf they mean)
        myDebugSerial->print("size:"); myDebugSerial->print(dataSize);
        myDebugSerial->print(" mode:"); myDebugSerial->print(mode);
        myDebugSerial->print(" command:0x"); myDebugSerial->print(command[0], HEX); myDebugSerial->print(",0x"); myDebugSerial->print(command[1], HEX);
        myDebugSerial->print(" params:");
        for (uint32_t i = 1; i <= paramsize; i++) {
          myDebugSerial->print(pParambytes[i - 1], HEX);
          myDebugSerial->print(",");
        }
        myDebugSerial->print(" csum:"); myDebugSerial->print(receivedchecksum, HEX);
      #endif
      byte expectedcsum = validChecksum();
      if (expectedcsum == receivedchecksum) {
        #if defined(DEBUG)
          myDebugSerial->print(" [OK]");
        #endif
      }
      else {
        #if defined(DEBUG)
          myDebugSerial->print(" [NG expected "); myDebugSerial->print(expectedcsum, HEX); myDebugSerial->print("]");
        #endif
        //##################OH YEAH SHOULD PROBABLY NOT PROCESS THE COMMAND IF THE CHECKSUM IS BAD DUH###############
        #if defined(DEBUG)
          myDebugSerial->println();
        #endif
        receiveState = WAITING_FOR_HEADER1;
        memset(dataBuffer, 0, sizeof(dataBuffer));  //do we need to clear buffer?
        break;    //should go to end of switch (receiveState) which is at the end of PodserialState::process() so "return;" would be the same
      }
      
      #if defined(DEBUG)
        myDebugSerial->println();
      #endif

      //here try to print out what the bytes mean
       switch (mode) {
        case MODE_SWITCHING_MODE:
          switch (command[0]) {
            case 0x01:
              #if defined(DEBUG)
                //myDebugSerial->print("switch to mode ");
                myDebugSerial->print("Identify, supported lingo:");
                myDebugSerial->println(command[1]);
              #endif
              //0xFF,0x55,0x04,0x00,0x02,0x00,0x05,0xF5 mystery response from ipod that i think may be confirmation of switching to mode4??
               //this is ACK SUCCESS for command that was received (which was 0x05, EnterRemoteUIMode)
              if (command[1] == 0x04) {
                currentMode = MODE4;
                //according to spec we should not ACK Identify
                //static const byte mode4confirm[] = {0x05};
                //send_response(MODE_SWITCHING_MODE, 0x02, 0x00, mode4confirm, 1);
                }
              break;
            case 0x03: //we don't care what command[1] is but it am guessing it will be 0x00?
              #if defined(DEBUG)
                myDebugSerial->println("get mode");
              #endif
              send_response(MODE_SWITCHING_MODE, 0x04, ADVANCED_REMOTE_MODE);  //response: "current mode is mode 4"
              break;
            case 0x04:
              #if defined(DEBUG)
                myDebugSerial->print("response: mode is:");
                myDebugSerial->println(command[1]);
              #endif
              break;
            case 0x05:  //we don't care what command[1] is but it should be 0x00? (was 0x00 from my car dock)
              #if defined(DEBUG)
                //myDebugSerial->println("switch to mode 4");
                myDebugSerial->println("EnterRemoteUIMode");
              #endif
              currentMode = MODE4;
              static const byte mode4confirm[] = {0x05};
              send_response(MODE_SWITCHING_MODE, 0x02, 0x00, mode4confirm, 1);
              break;
            case 0x06:
              #if defined(DEBUG)
                myDebugSerial->println("switch to mode 2");
              #endif
              break;
          }
          break;
        case SIMPLE_REMOTE_MODE:
          if (command[0] != 0x00) {break;}  //always supposed to be 0x00
          switch (paramsize) {
            case 0:
              switch (command[1]) {
                case 0x00: myDebugSerial->println("button released"); break;
                case 0x01: myDebugSerial->println("play/pause"); break;
                case 0x02: myDebugSerial->println("vol+"); break;
                case 0x04: myDebugSerial->println("vol-"); break;
                case 0x08: myDebugSerial->println("skip>"); break;
                case 0x10: myDebugSerial->println("skip<"); break;
                case 0x20: myDebugSerial->println("next album"); break;
                case 0x40: myDebugSerial->println("prev album"); break;
                case 0x80: myDebugSerial->println("stop"); break;
              }
            case 1:
              switch (pParambytes[0]) {
                case 0x01: myDebugSerial->println("play"); break;
                case 0x02: myDebugSerial->println("pause"); break;
                case 0x04: myDebugSerial->println("toggle mute"); break;
                case 0x20: myDebugSerial->println("next playlist"); break;
                case 0x40: myDebugSerial->println("prev playlist"); break;
                case 0x80: myDebugSerial->println("toggle shuffle"); break;
              }
            case 2:
              switch (pParambytes[1]) {
                case 0x01: myDebugSerial->println("toggle repeat"); break;
                case 0x04: myDebugSerial->println("ipod off"); break;
                case 0x08: myDebugSerial->println("ipod on"); break;
                case 0x40: myDebugSerial->println("menu"); break;
                case 0x80: myDebugSerial->println("OK/select"); break;
              }
            case 3:
              switch (pParambytes[2]) {
                case 0x01: myDebugSerial->println("scroll up"); break;
                case 0x02: myDebugSerial->println("scroll down"); break;
              }
          }
          break;
        case ADVANCED_REMOTE_MODE:
          if (command[0] != 0x00) {break;}  //always supposed to be 0x00
          #if defined(DEBUG)
            //strcpy_P(buffer, (char*)pgm_read_word(&(mode4CmdNames[command[1]]))); // print command name from mode4CmdNames array
            //myDebugSerial->print(buffer);
            //myDebugSerial->print( (char*)&(mode4CmdNames[command[1]]) );
            myDebugSerial->print( mode4CmdNames[command[1]] );
          #endif
          switch (command[1]) {
            case REQUEST_PROTOCOL_VERSION: //ReturnProtocolVersion: Returns the iPod Extended Interface protocol version number
              static const byte bVersion[] = {0x01, 0x00}; //v1.00 (using lowest allowed ver b/c then least amount of supported cmds to deal with)
              send_response(ADVANCED_REMOTE_MODE, 0x00, RETURN_PROTOCOL_VERSION, bVersion, 2);
              break;

            case CMD_GET_IPOD_NAME:
              send_response(ADVANCED_REMOTE_MODE, 0x00, RESPONSE_IPOD_NAME, "Fake Bluetooth iPod");
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
                  case 0x01:
                    #ifdef DEBUG
                      myDebugSerial->print("play/pause");
                    #endif
                    switch (playingState) {
                      case STATE_PLAYING:
                        if (allowPlayPausecmd && myBC127.BC127_status == myBC127.AVRCP_connected) {
                          myBC127.MusicSendCmd("MUSIC PAUSE");
                          //Going to set state here AND when receiving feedback event from BC127 b/c some apps DO NOT send play/pause feedback
                          playingState = STATE_PAUSED;
                          accumTrackPlaytime += now - trackstarttime;  //add track playing time when we pause
                          allowPlayPausecmd = false;
                        }
                        break;
                      case STATE_PAUSED: case STATE_STOPPED:
                        if (allowPlayPausecmd && myBC127.BC127_status == myBC127.AVRCP_connected) {
                          myBC127.MusicSendCmd("MUSIC PLAY");
                          //Going to set state here AND when receiving feedback event from BC127 b/c some apps DO NOT send play/pause feedback
                          playingState = STATE_PLAYING;
                          trackstarttime = now;
                          allowPlayPausecmd = false;
                        }
                        break;
                    }
                    break;
                  #ifdef DEBUG
                  case 0x02: myDebugSerial->print("stop"); break;
                  #endif
                  case 0x03:
                    #ifdef DEBUG
                      myDebugSerial->print("skip++");
                    #endif
                    myBC127.MusicSendCmd("MUSIC FORWARD");
                    //playlistpos++;
                    trackstarttime = now;  //save track start time as now
                    //send_response(ADVANCED_REMOTE_MODE, 0x00, RESPONSE_POLLING_MODE, 0x01, playlistpos - 1);
                    break;
                  case 0x04:
                    #ifdef DEBUG
                      myDebugSerial->print("skip--");
                    #endif
                    myBC127.MusicSendCmd("MUSIC BACKWARD");
                    //(playlistpos > 1) ? playlistpos-- : playlistpos = 99;   //wrap around to 99 if we are at 1 and try to decrement
                    trackstarttime = now;  //save track start time as now
                    accumTrackPlaytime = 0; //clear accum. track playtime
                    //send_response(ADVANCED_REMOTE_MODE, 0x00, RESPONSE_POLLING_MODE, 0x01, playlistpos - 1);
                    break;
                  #ifdef DEBUG
                  case 0x05: myDebugSerial->print("FFwd"); break;
                  case 0x06: myDebugSerial->print("Rwnd"); break;
                  case 0x07: myDebugSerial->print("StopFF/RW"); break;
                  #endif
                }
              send_feedback(CMD_PLAYBACK_CONTROL, RESULT_SUCCESS);
              break;
              
            case CMD_GET_TIME_AND_STATUS_INFO:
              send_response(ADVANCED_REMOTE_MODE, 0x00, RESPONSE_TIME_AND_STATUS_INFO, trackLength, 
                playingState==STATE_PLAYING ? ((now - trackstarttime) + accumTrackPlaytime) : accumTrackPlaytime, 
                playingState);
              //need to reset play/pause cmd disable here b/c this is when playing state is sent to dock
              //otherwise we end up changing it at another time and dock still has old information and we get another inappropriate play/pause toggle
              if (prevplayingState != playingState) {
                allowPlayPausecmd = true;
                #ifdef DEBUG
                  myDebugSerial->print("\nplayingstate changed");
                #endif
                }
              prevplayingState = playingState;
              break;

            case CMD_GET_PLAYLIST_POSITION:
              send_response(ADVANCED_REMOTE_MODE, 0x00, RESPONSE_PLAYLIST_POSITION, playlistpos - 1);
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

            case CMD_EXECUTE_SWITCH:
              send_feedback(CMD_EXECUTE_SWITCH, RESULT_SUCCESS);
              break;
              
            case CMD_SWITCH_TO_ITEM:
              #if defined(DEBUG)
                switch (pParambytes[0]) {
                  case TYPE_PLAYLIST: myDebugSerial->print("playlist"); break;
                  case TYPE_ARTIST: myDebugSerial->print("artist"); break;
                  case TYPE_ALBUM: myDebugSerial->print("album"); break;
                  case TYPE_GENRE: myDebugSerial->print("genre"); break;
                  case TYPE_SONG: myDebugSerial->print("song"); break;
                  case TYPE_COMPOSER: myDebugSerial->print("composer"); break;
                }
                myDebugSerial->print(" #"); myDebugSerial->print(endianConvert(pParambytes + 1));
              #endif
              send_feedback(CMD_SWITCH_TO_ITEM, RESULT_SUCCESS);
              break;
              
            case CMD_GET_ITEM_COUNT:
              #if defined(DEBUG)
                switch (pParambytes[0]) {
                  case TYPE_PLAYLIST: myDebugSerial->print("playlists"); break;
                  case TYPE_ARTIST: myDebugSerial->print("artists"); break;
                  case TYPE_ALBUM: myDebugSerial->print("albums"); break;
                  case TYPE_GENRE: myDebugSerial->print("genres"); break;
                  case TYPE_SONG: myDebugSerial->print("songs"); break;
                  case TYPE_COMPOSER: myDebugSerial->print("composers"); break;
                }
              #endif
              //just dummy info; tell it that all playlists have count of 5...does this even matter????
              //small number is better b/c it won't ask for size of like a billion playlists
              send_response(ADVANCED_REMOTE_MODE, 0x00, RESPONSE_ITEM_COUNT, 5);
              break;
              
            case CMD_GET_SONG_COUNT_IN_CURRENT_PLAYLIST:
              //just dummy info; tell it that song count is 1
              send_response(ADVANCED_REMOTE_MODE, 0x00, RESPONSE_SONG_COUNT_IN_CURRENT_PLAYLIST, 1);
              break;

            case CMD_GET_ITEM_NAMES:
              #if defined(DEBUG)
                switch (pParambytes[0]) {
                  case TYPE_PLAYLIST: myDebugSerial->print("playlists"); break;
                  case TYPE_ARTIST: myDebugSerial->print("artists"); break;
                  case TYPE_ALBUM: myDebugSerial->print("albums"); break;
                  case TYPE_GENRE: myDebugSerial->print("genres"); break;
                  case TYPE_SONG: myDebugSerial->print("songs"); break;
                  case TYPE_COMPOSER: myDebugSerial->print("composers"); break;
                }
                myDebugSerial->print(", starting item#"); myDebugSerial->print(endianConvert(pParambytes + 1));
                myDebugSerial->print(", num of items:"); myDebugSerial->print(endianConvert(pParambytes + 5));
              #endif
              start_num_to_get = endianConvert(pParambytes + 1);
              num_of_items_to_get = endianConvert(pParambytes + 5);
              //we include starting number as an item to get, i.e start at 1 and get 5 items is 1,2,3,4,5=5items
              for (int i = start_num_to_get; i < start_num_to_get + num_of_items_to_get; i++) {
                send_response(ADVANCED_REMOTE_MODE, 0x00, RESPONSE_ITEM_NAMES, i, "FAKE");
              }
              break;
              
            case CMD_GET_TITLE: case CMD_GET_ARTIST: case CMD_GET_ALBUM:
            #if defined(DEBUG)
              myDebugSerial->print(endianConvert(pParambytes));   //track number
            #endif
              switch (command[1]) {
                case CMD_GET_TITLE:
                  send_response(ADVANCED_REMOTE_MODE, 0x00, RESPONSE_TITLE, trackTitle);
                  //reset waiting for text request flag
                  isWaitingForNewTextRequest = false;
                  break;
                case CMD_GET_ARTIST: send_response(ADVANCED_REMOTE_MODE, 0x00, RESPONSE_ARTIST, trackArtist); break;
                case CMD_GET_ALBUM: send_response(ADVANCED_REMOTE_MODE, 0x00, RESPONSE_ALBUM, trackAlbum); break;
              }
              break;

            //param for these should be a string, display it
            case RESPONSE_TITLE: case RESPONSE_ARTIST: case RESPONSE_ALBUM: case RESPONSE_IPOD_NAME:
              #if defined(DEBUG)
                myDebugSerial->print("\""); myDebugSerial->print((char *)pParambytes); myDebugSerial->print("\"");
              #endif
              break;
              
            case RESPONSE_FEEDBACK:
              #if defined(DEBUG)
                //myDebugSerial->print("feedback for ");
                myDebugSerial->print(" for ");
                //2nd+3rd param bytes should be command that this is feedback for (and 1st byte of command should always be 0x00)
                //strcpy_P(buffer, (char*)pgm_read_word(&(mode4CmdNames[pParambytes[2]])));
                //myDebugSerial->print(buffer);
                //myDebugSerial->print( (char*)&(mode4CmdNames[pParambytes[2]]) );
                myDebugSerial->print( mode4CmdNames[pParambytes[2]] );
                myDebugSerial->print(" is "); myDebugSerial->print(pParambytes[0]);
                myDebugSerial->print("(");
                switch (pParambytes[0]) {
                  case 0x00: myDebugSerial->print("SUCCESS"); break;
                  case 0x02: myDebugSerial->print("FAILURE"); break;
                  case 0x04: myDebugSerial->print("LIMIT EXCEEDED/WRONG PARAM COUNT"); break;
                  default: myDebugSerial->print("??"); break;
                }
                myDebugSerial->print(")");
              #endif
              break;
              
            case RESPONSE_TIME_AND_STATUS_INFO:
              #if defined(DEBUG)
                myDebugSerial->print("track length:"); myDebugSerial->print(endianConvert(pParambytes)); myDebugSerial->print("ms ");
                myDebugSerial->print("elapsed time:"); myDebugSerial->print(endianConvert(pParambytes + 4)); myDebugSerial->print("ms ");
                myDebugSerial->print("status:");
                switch (pParambytes[8]) {
                  case STATE_STOPPED: myDebugSerial->print("stopped"); break;
                  case STATE_PLAYING: myDebugSerial->print("playing"); break;
                  case STATE_PAUSED: myDebugSerial->print("paused"); break;
                }
              #endif
              break;
                        
            case RESPONSE_PLAYLIST_POSITION:
              #if defined(DEBUG)
                myDebugSerial->print(endianConvert(pParambytes));
              #endif
              break;
            case RESPONSE_POLLING_MODE:
              #if defined(DEBUG)
                if (pParambytes[0] == 0x01) {myDebugSerial->print("Track changed to #"); myDebugSerial->print(endianConvert(pParambytes + 1));}
                else {myDebugSerial->print("elapsed time is:"); myDebugSerial->print(endianConvert(pParambytes + 1));}
              #endif
              break;
          }
          #if defined(DEBUG)
            myDebugSerial->println();
          #endif
          break;
      }
      
      #if defined(DEBUG)
        myDebugSerial->println();
      #endif
      
      receiveState = WAITING_FOR_HEADER1;
      memset(dataBuffer, 0, sizeof(dataBuffer));  //do we need to clear buffer?
                                                  // we should know how many bytes we got and they should be overwritten...
      break;
  }
}
