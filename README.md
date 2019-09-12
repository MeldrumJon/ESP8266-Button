# ESP8266-LIFX-Switch

A battery-powered "light switch" which toggles a LIFX bulb's power through the 
[LIFX LAN Protocol](https://lan.developer.lifx.com/).

## Parts

- ESP8266-01S module
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
2. Open the ESP8266-LIFX-Switch project folder in VSCode.
3. Install the [Arduino extension](https://marketplace.visualstudio.com/items?itemName=vsciot-vscode.vscode-arduino) for VSCode.
3. Connect the serial adapter's TX signal to RX on the ESP8266, RX to TX and GND to GND.
    - There's no GND pin header on the PCB, instead connect an alligator clip to the voltage regulator's tab.
4. Update the WiFi SSID, IP addresses, and LIFX Bulb's MAC address in the `Addresses and Settings` section at the top of `main.cpp`.

## Programming

1. Set the slide switch to hold GPIO0 low.
2. In VSCode, press F1 and run command "Arduino: Upload"
3. Once esptool.py begins trying to connect, press the pushbutton (momentarily hold RST low).
4. Move the slide switch back to hold GPIO0 high.

## PCB

KiCAD files are in [the `PCB` Folder](./PCB).  PCB made at [OSH Park](https://oshpark.com/).

![Manufactured PCB](../assets/PCB.jpg?raw=true)

