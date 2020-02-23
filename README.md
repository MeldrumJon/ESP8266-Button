# ESP8266-Button

Battery-powered WiFi button.

Can be used as an [IFTTT](https://ifttt.com/) trigger through 
[Webhooks](https://ifttt.com/maker_webhooks) or to 
control LIFX smart lights through the 
[LIFX HTTP API](https://api.developer.lifx.com/) or 
[LIFX LAN Protocol](https://lan.developer.lifx.com/).


![Button with Enclosure](../assets/Button.png?raw=true)

## Parts

- ESP8266-01 module
- Low quiescent current LDO 3.3V voltage regulator ([MCP1826S-3302E/AB](https://www.mouser.com/ProductDetail/579-MCP1826S-3302EAB))
- 4.7uF electrolytic capacitor
- 1uF electrolytic capacitor
- Momentary pushbutton switch
- SPDT mini slide switch, 3-pin, 2.54mm pin spacing
- 2x 1k resistors
- Battery holder for 3xAA or 3xAAA batteries
- Breakaway headers (optional, for ESP8266-01 module)
- 2-pin JST connector cables (optional, useful for buttons or batteries)
- Project Box 100x60x25 mm; inside dimensions: 96x56x23 mm

## Tools

- USB to TTL Serial **3.3V** adapter

## PCB

KiCAD files are in [the `hardware` Folder](./hardware).

![ESP8266 LIFX Switch Schematic](../assets/Schematic.png?raw=true)

## Assembled Box

![ESP8266-Button Assembled](../assets/Assembly.png?raw=true)


## Programming

### Setup

1. [Install the ESP8266 Arduino Core](https://github.com/esp8266/Arduino#installing-with-boards-manager)
2. Open one of the projects in [src/](./src/) in Arduino IDE.
3. In Arduino IDE,
	- Set `Tools->Board` to "Generic ESP8266 Module".
	- Set `Tools->Port` to your USB TTL Serial Adapter.
4. Connect the serial adapter's TX signal to RX on the ESP8266, RX to TX and 
GND to GND.
5. Update the `WIFI_SSID`, `WIFI_PASSWORD` and any other variables/macros marked
"TODO" to match your WiFi settings.

### Uploading

1. Set the slide switch to hold GPIO0 low.
2. Upload the sketch.
3. Whenever esptool.py tries to connect, press the pushbutton (to momentarily
hold RST low).
4. Wait for esptool to finish programming the ESP8266.
5. Move the slide switch back to hold GPIO0 high.

