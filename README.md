# WeMo compatible switches on an ESP8266 WiFi board
Create your own Alexa capable switch for under $10

This has been tested with a NodeMCU v1.0 (USB based dev kit) as well as a LinkNode R4 (serial with 4 relays)

## Getting started

### Set up Arudino

1. Install the [Arduino software](https://www.arduino.cc/en/Main/Software)
2. Add **ESP8266** board support to Arduino by adding `http://arduino.esp8266.com/stable/package_esp8266com_index.json` to the Arduino app preferences *Additional Boards Manager URLs*
3. Add **TimeLib** library to Arduino by [downloading latest zip](https://github.com/PaulStoffregen/Time/releases) and adding that download to Arduino using *Sketch > Include Library > Add .ZIP library*
4. Add **WiFiManager** library to Arduino by using *Sketch > Include Library > Manage Librarys* then searching for *WiFiManager by tzapu* and pressing *Install*
4. Open the esp8266-multi-wemo.ino file in Arduino and *Sketch > Verify/Compile*

If you have no errors you are ready to proceed!

### Flash for the first time

Flashing the boards varies a little primarily because the ESP8266 doesn't include USB communication but rather uses a UART interface. Here's the two I've used;

#### NodeMCU 1.0 ESP-12E

The NodeMCU includes a SiLabs SI2102 USB chip to handle the UART communication with the ESP8266. Install the SiLabs [USB to UART driver compatible with this board](http://www.silabs.com/products/mcu/pages/usbtouartbridgevcpdrivers.aspx)

Choose the following settings from the Tools menu in Arduino: 

* **Board** "NodeMCU 1.0 (ESP-12E Module)"
* **CPU Frequency** "80 Mhz"
* **Flash Size** "4M (3M SPIFFS)"
* **Upload Speed** "460800" (Lower this if you get errors)
* **Port** "/dev/cu.SLAB_USBtoUART" (Mac, Windows will be different)

#### LinkNode R4

This board does not include a USB to UART converter so you'll need a suitable cable and the appropriate drivers. I used the Adafruit 954 the older model of which uses a chip by [Prolific and it's own drivers](http://www.prolific.com.tw/US/ShowProduct.aspx?p_id=229&pcid=41). Newer versions of this cable use the SiLabs chipset and drivers described in the NodeMCU section above.

Choose the following settings from the Tools menu in Arduino: 

* **Board** "Generic ESP8266 Module"
* **CPU Frequency** "80 Mhz"
* **Flash Size** "4M (3M SPIFFS)"
* **Upload Speed** "115200" (Lower this if you get errors)
* **Port** "/dev/tty.usbserial" (Mac, Windows will be different)

Note that the generic board doesn't include device-specific pinouts so you'll need to find out what IO pins are wired to what and make appropriate changes to the source code by adding a section to the top of the source after the #includes for each pin.

`const int D1 = 13;`

## Credits

Based on [code by Kakopappa](https://github.com/kakopappa/arduino-esp8266-alexa-wemo-switch)

Subsequently added:

* WiFi Manager support and SoftAP mode to avoid hard-coded WiFi credentials
* NTP time-server support and necessary changes to headers
* Various code refactorings
