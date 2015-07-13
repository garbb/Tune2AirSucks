/* UART Example, any character received on either the real
   serial port, or USB serial (or emulated serial to the
   Arduino Serial Monitor when using non-serial USB types)
   is printed as a message to both ports.

   This example code is in the public domain.
*/

// set this to the hardware serial port you wish to use
#define HWSERIAL Serial1

void setup() {
	Serial.begin(9600);
        HWSERIAL.begin(9600);
}

void loop() {
        int incomingByte;
        
	if (Serial.available() > 0) {
		incomingByte = Serial.read();
		//Serial.print("USB received: ");
   	Serial.write(incomingByte);
                //HWSERIAL.print("USB received:");
                HWSERIAL.write(incomingByte);
	}
	if (HWSERIAL.available() > 0) {
		incomingByte = HWSERIAL.read();
		//Serial.print("UART received: ");
		Serial.write(incomingByte);
                //HWSERIAL.print("UART received:");
                //HWSERIAL.println(incomingByte, DEC);
	}
}

