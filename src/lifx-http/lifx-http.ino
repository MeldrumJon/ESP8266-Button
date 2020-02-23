// Based on https://github.com/RuiSantosdotme/Random-Nerd-Tutorials/blob/master/Projects/ESP8266_WiFi_Button.ino

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

/*
 * Addresses and Settings
 */

// WiFi Settings
// TODO: Replace with your WiFi name and password
const char* const WIFI_SSID = "XXXXX";
const char* const WIFI_PASSWORD = "XXXXXXXX";

// Static IP address (for speed: using DHCP takes awhile)
// TODO: Set IP, GATEWAY, SUBNET, DNS to match router settings
// TODO: Assign a static IP to this device on your router
IPAddress IP(192, 168, 1, 71);
IPAddress GATEWAY(192, 168, 1, 1);
IPAddress SUBNET(255, 255, 255, 0);
IPAddress DNS(192, 168, 1, 1);

// TODO: Update API key and change resource to desired command
const char* const SERVER = "api.lifx.com";
const char* const RESOURCE = "/v1/lights/group:Porch/toggle";
const char* const API_KEY = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";

/*
 * HTTPS Setup
 */

#define PORT 443
WiFiClientSecure client;

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
    WiFi.mode(WIFI_STA);
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
        Serial.println("Could not connect to WiFi.");
        return; // Go to sleep
    }

    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    // Don't worry about certificates
    client.setInsecure();

    // Connect to IFTTT web service
    Serial.print("Connecting to ");
    Serial.println(SERVER);
    if (!client.connect(SERVER, PORT)) {
        Serial.println(String("Could not connect to ") + SERVER);
        return; // Go to sleep
    }
    Serial.print("Requesting resource ");
    Serial.println(RESOURCE);
    client.print(String("POST ") + RESOURCE + " HTTP/1.1\r\n" +
                  "Host: " + SERVER + "\r\n" + 
                  "Authorization: Bearer " + API_KEY + "\r\n" +
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
