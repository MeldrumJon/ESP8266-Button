// Based on https://github.com/RuiSantosdotme/Random-Nerd-Tutorials/blob/master/Projects/ESP8266_WiFi_Button.ino

#include <Arduino.h>
#include <ESP8266WiFi.h>

/*
 * Addresses and Settings
 */

// WiFi Settings
const char* const WIFI_SSID = "wifi_name"; // TODO: replace with your SSID
const char* const WIFI_PASSWORD = "password"; // TODO: replace with your password

// Static IP address (for speed: using DHCP takes awhile)
// TODO: Set IP, GATEWAY, SUBNET, DNS to match router settings
IPAddress IP(192, 168, 1, 71); // TODO: assign a static IP to this device on your router
IPAddress GATEWAY(192, 168, 1, 1);
IPAddress SUBNET(255, 255, 255, 0);
IPAddress DNS(192, 168, 1, 1);

// TODO: these should match the URL for your webhooks event. Update the trigger and API key.
const char* const SERVER = "maker.ifttt.com";
const char* const RESOURCE = "/trigger/btn_press/with/key/mYALpHanuMEriCkEY";

/*
 * HTTP Setup
 */

#define PORT 80
WiFiClient client;

/*
 * Wifi Setup
 */

void setup()
{
    unsigned long timeout;
  
    // Keep light on to show that the ESP8266 is not sleeping
    pinMode(2, OUTPUT);
    digitalWrite(2, LOW);

    Serial.begin(115200);
    //Serial.setDebugOutput(true);
    Serial.println();
    Serial.print("MAC: ");
    Serial.println(WiFi.macAddress());
    Serial.println("Connecting to WiFi...");

    // Connect to WiFi
    WiFi.config(IP, GATEWAY, SUBNET, DNS); // Comment out to use DHCP
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    timeout = millis() + 10000; // wait 10s
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() > timeout) {
            break;
        }
        delay(1); // keep WDT happy
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Could not connect.");
        return; // Go to sleep
    }

    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    // Connect to IFTTT web service
    Serial.print("Connecting to ");
    Serial.println(SERVER);
    if (!client.connect(SERVER, PORT)) {
        Serial.println("Could not connect!");
        return; // Go to sleep
    }
    Serial.print("Requesting resource ");
    Serial.println(RESOURCE);
    client.print(String("GET ") + RESOURCE + 
                  " HTTP/1.1\r\n" +
                  "Host: " + SERVER + "\r\n" + 
                  "Connection: close\r\n\r\n");

    // Wait for Response
    timeout = millis() + 5000; // wait 5s
    while (!client.available()) {
        if (millis() > timeout) {
            return; // Go to sleep
        }
        delay(1); // keep WDT happy
    }
    // Print response
    Serial.println("RESPONSE");
    while (client.available()) {
        char c = client.read();
        Serial.write(c);
    }

    return; // Go to sleep.
}

void loop()
{
    ESP.deepSleep(0); // Sleep forever
}
