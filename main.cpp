// Based on https://github.com/aijayadams/esp8266-lifx

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

/*
 * Addresses and Settings TODO: update these with your settings
 */

// WiFi Settings
const char *WIFI_SSID = "id";
const char *WIFI_PASSWORD = "pwd";

// Static IP address (DHCP can take too long)
IPAddress ip(192,168,1,71);   
IPAddress gateway(192,168,1,1);   
IPAddress subnet(255,255,255,0);   
IPAddress bcast(192,168,1,255);

// LIFX MAC Address
#define LIFX_TARGET {0xD0, 0x73, 0xD5, 0x30, 0x05, 0x45, 0x00, 0x00}


/*
 * WiFi Setup
 */

#define BUFFER_LEN 128
#define LIFX_UDP_PORT 56700
WiFiUDP UDP;
char packetBuffer[BUFFER_LEN];


/*
 * LIFX Packets
 */

#define LIFX_HEADER_TARGET_LEN 8
#define LIFX_HEADER_RESERVED_LEN 6

#pragma pack(push, 1)
typedef struct {
	/* frame */
	uint16_t size;            // Size of entire message in bytes including this field
	uint16_t protocol : 12;   // Protocol number: must be 1024 (decimal)
	uint8_t addressable : 1;  // Message includes a target address: must be one (1)
	uint8_t tagged : 1;       // Determines usage of the Frame Address target field
	uint8_t origin : 2;       // Message origin indicator: must be zero (0)
	uint32_t source;          // Source identifier: unique value set by the client, used by responses
	/* frame address */
	uint8_t target[LIFX_HEADER_TARGET_LEN];        // 6 byte device address (MAC address) or zero (0) means all devices
	uint8_t reserved[LIFX_HEADER_RESERVED_LEN];    // Must all be zero (0)
	uint8_t res_required : 1; // Response message required
	uint8_t ack_required : 1; // Acknowledgement message required
	uint8_t : 6;              // Reserved
	uint8_t sequence;         // Wrap around message sequence number
	/* protocol header */
	uint64_t : 64;            // Reserved
	uint16_t type;            // Message type determines the payload being used
	uint16_t : 16;            // Reserved
	/* variable length payload follows */
} lx_protocol_header_t;
#pragma pack(pop)

// Messages
#pragma pack(push, 1)
typedef struct {
	uint8_t service;
	uint32_t port;
} lx_msg_stateService_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
	uint16_t level;
	uint32_t duration;
} lx_msg_setPower_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
	uint16_t level;
} lx_msg_statePower_t;
#pragma pack(pop)

static void print_header(lx_protocol_header_t* head) {
	Serial.println("HEADER");
	Serial.printf("Size: %d\n", head->size);
	Serial.printf("Protocol: %d\n", head->protocol);
	Serial.printf("Addressable: %d\n", head->addressable);
	Serial.printf("Tagged: %d\n", head->tagged);
	Serial.printf("Origin: %d\n", head->origin);
	Serial.printf("Source: %d\n", head->source);
	Serial.printf("Target: ");
	for (uint8_t i = 0; i < LIFX_HEADER_TARGET_LEN; ++i) {
		Serial.printf("%02X ", head->target[i]);
	}
	Serial.println();
	Serial.printf("Reserved: ");
	for (uint8_t i = 0; i < LIFX_HEADER_RESERVED_LEN; ++i) {
		Serial.printf("%02X ", head->reserved[i]);
	}
	Serial.println();
	Serial.printf("Response Required: %d\n", head->res_required);
	Serial.printf("Acknowledgement Required: %d\n", head->ack_required);
	Serial.printf("Sequence: %d\n", head->sequence);
	Serial.printf("Type: %d\n", head->type);
	Serial.println();
}

static void print_msgSetPower(lx_msg_setPower_t* msg) {
	Serial.println("MESSAGE: SetPower");
	Serial.printf("Level: %d\n", msg->level);
	Serial.printf("Duration: %d\n", msg->duration);
	Serial.println();
}

static void print_msgStateService(lx_msg_stateService_t* msg) {
	Serial.println("MESSAGE: StateService");
	Serial.printf("Service: %d\n", msg->service);
	Serial.printf("Port: %d\n", msg->port);
	Serial.println();
}

static void print_msgStatePower(lx_msg_statePower_t* msg) {
	Serial.println("MESSAGE: StatePower");
	Serial.printf("Level: %d\n", msg->level);
	Serial.println();
}

/*
 * Defined LIFX Messages
 */

#define LIFX_SOURCE 3549

#define LIFX_TYPE_DISCOVERY 2
#define LIFX_TYPE_STATESERVICE 3
#define LIFX_TYPE_GETPOWER 20
#define LIFX_TYPE_SETPOWER 21
#define LIFX_TYPE_STATEPOWER 22
#define LIFX_TYPE_ACKNOWLEDGEMENT 45

static lx_protocol_header_t discovery_header = {
	sizeof(lx_protocol_header_t), // Size
	1024, // Protocol
	1, // Addressable
	1, // Tagged
	0, // Origin
	LIFX_SOURCE, // Source
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Target
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Reserved
	0, // Response Required
	0, // Acknowledge Requried
	0, // Sequence
	LIFX_TYPE_DISCOVERY // Type (Discovery)
};

static lx_protocol_header_t getPower_header = {
	sizeof(lx_protocol_header_t), // Size
	1024, // Protocol
	1, // Addressable
	0, // Tagged
	0, // Origin
	LIFX_SOURCE, // Source
	LIFX_TARGET, // Target
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Reserved
	1, // Response Requested
	0, // Acknowledge Requested
	0, // Sequence
	LIFX_TYPE_GETPOWER // Type (GetPower)
};

static lx_protocol_header_t setPower_header = {
	sizeof(lx_protocol_header_t)+sizeof(lx_msg_setPower_t),
	1024, // Protocol
	1, // Addressable
	0, // Tagged
	0, // Origin
	LIFX_SOURCE, // Source
	LIFX_TARGET, // Target
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Reserved
	0, // Response Requested
	1, // Acknowledge Requested
	0, // Sequence
	LIFX_TYPE_SETPOWER // Type (SetPower)
};

static lx_msg_setPower_t setPower_msg = {
	65535 // Level
};

/*
 * LIFX Functions
 */

// #define LIFX_DEBUG

uint8_t lifx_discover() {
	uint8_t found = 0;

	UDP.beginPacket(bcast, LIFX_UDP_PORT);
    UDP.write( (char*) &discovery_header, sizeof(lx_protocol_header_t));
    UDP.endPacket();

  	unsigned long started = millis();
  	while (millis() - started < 250) { // Only search for 250ms
  	  	int packetLen = UDP.parsePacket();
  	  	if (packetLen && packetLen < BUFFER_LEN) {
  	  		int read = UDP.read(packetBuffer, packetLen);
			#ifdef LIFX_DEBUG
			Serial.printf("PacketLen: %d, read: %d\r\n", packetLen, read);
			#endif
			lx_protocol_header_t* header = (lx_protocol_header_t*) packetBuffer;
			lx_msg_stateService_t* payload = (lx_msg_stateService_t*) (packetBuffer + sizeof(lx_protocol_header_t));
			#ifdef LIFX_DEBUG
			print_header(header);
			print_msgStateService(payload);
			#endif
  	  		if (header->type == LIFX_TYPE_STATESERVICE) {
  	  	    	++found;
  	  	  	} else {
				#ifdef LIFX_DEBUG
  	  	    	Serial.print("Unexpected Packet type: ");
  	  	    	Serial.println(((lx_protocol_header_t*)packetBuffer)->type);
				#endif
  	  	  	}
  	  	}
  	}

	return found;
}

uint16_t lifx_getPower() {
  	uint16_t power = 0;

	// Try a few times
	for (uint8_t i = 0; i < 8; ++i) { // 2 seconds for response
  		UDP.beginPacket(bcast, LIFX_UDP_PORT);
  		UDP.write((char *) &getPower_header, sizeof(lx_protocol_header_t));
  		UDP.endPacket();

  		unsigned long started = millis();
  		while (millis() - started < 250) { // This ones important, wait 750ms for a response.
  		  	int packetLen = UDP.parsePacket();
  		  	if (packetLen && packetLen < BUFFER_LEN) {
  		  		int read = UDP.read(packetBuffer, packetLen);
				#ifdef LIFX_DEBUG
				Serial.printf("PacketLen: %d, read: %d\r\n", packetLen, read);
				#endif
				lx_protocol_header_t* header = (lx_protocol_header_t*) packetBuffer;
				lx_msg_statePower_t* payload = (lx_msg_statePower_t*) (packetBuffer + sizeof(lx_protocol_header_t));
				#ifdef LIFX_DEBUG
				print_header(header);
				print_msgStatePower(payload);
				#endif
  		  		if (header->type == LIFX_TYPE_STATEPOWER) {
  		  	    	power = payload->level;
  		  	    	return power;
  		  	  	} else {
					#ifdef LIFX_DEBUG
  		  	    	Serial.print("Unexpected Packet type: ");
  		  	    	Serial.println(((lx_protocol_header_t*)packetBuffer)->type);
					#endif
  		  	  	}
  	 	 	}
  		}
	}
	
  	return power;
}

void lifx_setPower(uint16_t power) {
	setPower_msg.level = power;

	// Try a few times
	for (uint8_t i = 0; i < 4; ++i) {
  		UDP.beginPacket(bcast, LIFX_UDP_PORT);
  		UDP.write((char*) &setPower_header, sizeof(lx_protocol_header_t));
		UDP.write((char*) &setPower_msg, sizeof(lx_msg_setPower_t));
  		UDP.endPacket();

  		unsigned long started = millis();
  		while (millis() - started < 250) { // This ones important, wait 1s for an acknowledgement.
  		  	int packetLen = UDP.parsePacket();
  		  	if (packetLen && packetLen < BUFFER_LEN) {
  		  		int read = UDP.read(packetBuffer, packetLen);
				#ifdef LIFX_DEBUG
				Serial.printf("PacketLen: %d, read: %d\r\n", packetLen, read);
				#endif
				lx_protocol_header_t* header = (lx_protocol_header_t*) packetBuffer;
				#ifdef LIFX_DEBUG
				print_header(header);
				#endif
  		  		if (header->type == LIFX_TYPE_ACKNOWLEDGEMENT) {
  		  	    	return;
  		  	  	} else {
					#ifdef LIFX_DEBUG
  		  	    	Serial.print("Unexpected Packet type: ");
  		  	    	Serial.println(((lx_protocol_header_t*)packetBuffer)->type);
					#endif
  		  	  	}
  	 	 	}
  		}
	}
	return;
}

/*
 * Wifi Setup
 */

void setup() {
	// Keep light on to show that the ESP8266 is not sleeping
	pinMode(2, OUTPUT);
	digitalWrite(2, LOW);
	// WiFi.setAutoConnect(false);   // Not working by its own
	// WiFi.disconnect();  //Prevent connecting to wifi based on previous configuration

	// Hello world!
	Serial.begin(57600);
	Serial.println();
	Serial.print("MAC: ");
	Serial.println(WiFi.macAddress());
	Serial.println("Connecting to WiFi...");

	// Connect to WiFi
	WiFi.config(ip, gateway, subnet);
	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
	for (uint16_t i = 0; i < 100; ++i) { // Wait 10s
		if (WiFi.status() == WL_CONNECTED) { break; }
		delay(100);
	}
	if (WiFi.status() != WL_CONNECTED) { 
		Serial.println("Could not connect.");
		return; // Go to loop and sleep
	}

	Serial.println("WiFi connected");
	Serial.print("IP address: ");
	Serial.println(WiFi.localIP());

	// Toggle light
	UDP.begin(LIFX_UDP_PORT);

	uint16_t pow = lifx_getPower();
	Serial.printf("Power level is %d.\r\n", pow);
	if (pow > 32768) { pow = 0; }
	else { pow = 65535; }
	Serial.printf("Setting power to %d.\r\n", pow);
	lifx_setPower(pow);
	return;
}

void loop() {
	ESP.deepSleep(0); // Sleep forever
}