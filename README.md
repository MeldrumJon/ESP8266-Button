# ESP8266-Button

A battery-powered WiFi button using an ESP8266-01 module.  Can be used as an [IFTTT](https://ifttt.com/) trigger through 
[Webhooks](https://ifttt.com/maker_webhooks) or to 
control a LIFX bulb's power through the 
[LIFX LAN Protocol](https://lan.developer.lifx.com/).


## Parts

- ESP8266-01 module
- Low quiescent current LDO 3.3V voltage regulator ([MCP1826S-3302E/AB](https://www.mouser.com/ProductDetail/579-MCP1826S-3302EAB))
- 4.7uF electrolytic capacitor
- 1uF electrolytic capacitor
- Momentary pushbutton switch
- SPDT mini slide switch, 3-pin, 2.54mm (0.1") pin spacing
- 2x 1k resistors
- Breakaway headers
- Battery holder for 3xAA or 3xAAA batteries

## Tools

- USB to TTL Serial **3.3V** adapter

## Schematic

![ESP8266 LIFX Switch Schematic](../assets/Schematic.png?raw=true)

## Setup

1. [Install the ESP8266 Arduino Core](https://github.com/esp8266/Arduino#installing-with-boards-manager)
2. Open one of the projects in [src/](./src/) in Arduino IDE.
3. In Arduino IDE,
	- Set `Tools->Board` to "Generic ESP8266 Module".
	- Set `Tools->Port` to your USB TTL Serial Adapter.
4. Connect the serial adapter's TX signal to RX on the ESP8266, RX to TX and 
GND to GND.
	- On the PCB, RX is the pin closest to the ESP8266-01's antenna.
    - There's no GND pin header on the PCB, instead connect an alligator clip 
 	to the voltage regulator's tab.
5. Update the `WIFI_SSID`, `WIFI_PASSWORD` and any other variables/macros marked
"TODO" to match your WiFi settings.

## Programming

1. Set the slide switch to hold GPIO0 low.
	- On the PCB, slide towards the ESP8266-01 antenna.
2. Upload the sketch.
3. Whenever esptool.py tries to connect, press the pushbutton (to momentarily
hold RST low).
4. Wait for esptool to finish programming the ESP8266.
5. Move the slide switch back to hold GPIO0 high.
	- On the PCB, slide away from the ESP8266-01 antenna.

## PCB

KiCAD files are in [the `hardware` Folder](./hardware).  PCB made at [OSH Park](https://oshpark.com/).

![Manufactured PCB](../assets/PCB.jpg?raw=true)

