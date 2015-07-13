# ipod.sniff
This is an attempt to create an interface between the 30pin ipod connector in my car and bluetooth so that I can stream audio from my phone.

It uses a teensy 3.1 https://www.pjrc.com/teensy/teensy31.html arduino-type board and a BC127 chip https://www.sparkfun.com/products/11924 to manage the bluetooth connection.

It will support audio streaming over A2DP and remote control over AVRCP and also transmit song title/artist/album metadata if your phone has AVRCP 1.3 or greater.

"bt.to.ipod.dock" is the code for the ipod to bluetooth interface.<br>
"ipod.dock.sniffer" is just for sniffing the traffic between an ipod and a dock so I could figure out how it worked.



Some code/knowledge borrowed/stolen from the following:
* Arduino library for controlling ipods: https://github.com/finsprings/arduinaap<br>
* BC127 Bluetooth chip arduino library from sparkfun: https://github.com/sparkfun/SparkFun_BC127_Bluetooth_Module_Arduino_Library/
