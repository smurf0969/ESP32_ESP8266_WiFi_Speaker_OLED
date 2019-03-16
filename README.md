# ESP32 & ESP8266 WiFi Speaker with OLED display
A MP3 streaming WiFi speaker for ESP8266 & ESP32 chips

## Overview
During a recent pantomime production we were unable to get the internal building sound system working and had to come up with a cheap solution to pipe music into the dressing rooms.
Cobbling together a Raspberry Pi with sound card, ffmpeg and node rmtp server we were able to stream live AAC audio to a couple of mobiles running VLC Player. However due to the limitations of running live streaming on a PI, poor WiFi in the building and the buffering lag of VLC we ended up with a stuttering live stream that could be upto 40 seconds behind depending on the numer of connections, which was difficult for the cast and callers.

Having a couple of node mcu ESP8266 12E development boards that were destined for some extra home automation, I decided to see if it was possible to make a WiFi speaker that would perform better than our previous solution.
That was the basis for my first attempt [ESP8266_WiFi_Speaker](https://github.com/smurf0969/ESP8266_WiFi_Speaker) and after puchasing a Heltec WiFi Kit ESP32 with an onboard OLED display and having problems with wifi managers I decided to create a new wifi manager library [WiFiConnect](https://github.com/smurf0969/WiFiConnect) and convert my previous project to support both chips.

## Components  
   ### ESP8266
       * ESP8266 ESP-12E Development Board NodeMcu
       * 2x 4ohm 3W Loudspeaker
       * Mini 3W+3W DC 5V Audio Amplifier PAM8043
       * I2S PCM5102 DAC Decoder ( [ESP8266Audio by Earle F. Philhower, III](https://github.com/earlephilhower/ESP8266Audio)
                                    does allow for no decoder, but I have not tried it. )
       * 0.96" I2C IIC Serial 128X64 White OLED LCD LED 

       First major component problem was figuring out the DAC connections
       to use with AudioOutputI2S in the [ESP8266Audio](https://github.com/earlephilhower/ESP8266Audio) library.  
       The configuration I finnally manages to get to work was:  
          > 3.3V from ESP8266 -> VCC, 33V, XMT  
          > GND from ESP8266 -> GND, FLT, DMP, FMT, SCL  
          > GPIO15 (D8,TXD2) from ESP8266 -> BCK  
          > GPIO3 (RX,RXD0) from ESP8266 -> DIN  
          > GPIO2 (D4, TXD1) from ESP8266 -> LCK  
   
        The 5V Audio Amplifier PAM8043 needs a lot of juice and I found that as mine has a potentiometer I needed
        to turn it fully down to be able to program or run the ESP when only connected by USB serial connection.
        Powering from a USB power adapter for normal use, I had no problems.

   ### ESP32  
        * Heltec WiFi Kit ESP32 with an onboard 128x64 OLED display
        * 2x 4ohm 3W Loudspeaker
        * Mini 3W+3W DC 5V Audio Amplifier PAM8043

        As the ESP32 has 2 onboard DAC's I decided to use them rather than use another decoder.

## IDE settings & Library Versions
Please check/review instructions and screenshots in the [Wiki](https://github.com/smurf0969/ESP32_ESP8266_WiFi_Speaker_OLED/wiki)

## Known Issues
 * Using buffering for the stream can cause alot of buffer underruns causing alot of stutter.
 * With newer libraries installed compared to the previous project I have been unable to get a stable connection using the ESP12E unless I set the IwIP Variant to IPv6 Higher Bandwidth.
 * Powering off of a com port can cause issues unless the amplifier is turned right down or off for uploading and playing music at higher volumes.
 * **As I can't seem to get the project to compile in Travis CI for ESP8266 NodeMCU board,** 
   **I have added precomiled binaries to the initial release and hope to post instructions in the WiKi soon.**

## Putting it all together
Due to the possibility of frequent IP changes of my Raspberry Pi server I have used [WiFiConnect](https://github.com/smurf0969/WiFiConnect) and its custom paramters.
After trying lots of different configurations for the server mp3 encoding(ffmpeg,darkice) and distribution(icecast2,shoutcast), I have settled on using [Icecast2](http://icecast.org/) and [DarkIce](http://www.darkice.org/) with the following [Darkice configuration](./docs/darkice.cfg).

## Thanks
Many thanks to the authors and contibutors for the main libraries that made this project possible  
  - ArduinoJson 6.9.1 [https://github.com/bblanchon/ArduinoJson.git](https://github.com/bblanchon/ArduinoJson.git) or [https://blog.benoitblanchon.fr](https://blog.benoitblanchon.fr)
  - ESP8266Audio 1.1.3 [https://github.com/earlephilhower/ESP8266Audio](https://github.com/earlephilhower/ESP8266Audio)
  - ESP8266 and ESP32 Oled Driver for SSD1306 display 4.0.0 [https://github.com/ThingPulse/esp8266-oled-ssd1306](https://github.com/ThingPulse/esp8266-oled-ssd1306) (I use a modified version,see [WiFiConnect]())

Stuart Blair (smurf0969)
