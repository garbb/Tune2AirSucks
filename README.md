# ipod.sniff
This is an attempt to create an interface between the 30pin ipod connector in my car and bluetooth so that I can stream audio from my phone.

It uses a teensy 3.1 https://www.pjrc.com/teensy/teensy31.html arduino-type board and a BC127 chip https://www.sparkfun.com/products/11924 to manage the bluetooth connection.

It will support audio streaming over A2DP and remote control over AVRCP and also transmit song title/artist/album metadata if your phone has AVRCP 1.3 or greater.
