// https://github.com/aijayadams/esp8266-lifx

#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#define DEBUG 1

// Header
#define LIFX_HEADER_TARGET_LEN 8 
#define LIFX_HEADER_RESERVED_LEN 6
// Header Constants
#define LIFX_HEADER_PROTOCOL 1024 // Must be 1024
#define LIFX_HEADER_ADDRESSABLE 1 // Indicates header includes target address, must be 1
#define LIFX_HEADER_ORIGIN 0      // Must be 0 
// Header Parameters
#define LIFX_HEADER_SOURCE 3549
#define LIFX_HEADER_RES_REQ 0 // Have light send back an acknowledgement. Not used in this code.
#define LIFX_HEADER_ACK_REQ 0 // Request a response from the light. Not used in this code.

#define LIFX_MSG_GET_SERVICE 2
#define LIFX_MSG_SET_POWER 117

#define LIFX_UDP_PORT 56700 // Can be changed for newer bulbs, best compatibility use 56700

const char *WIFI_SSID = "125 2.4GHz";
const char *WIFI_PASSWORD = "fivetimesfive";
#define LIFX_TARGET {0xD0, 0x73, 0xD5, 0x30, 0x05, 0x45, 0x00, 0x00}

IPAddress bcastAddr(255, 255, 255, 255);
int lxPort = 56700;

WiFiUDP UDP;

int8_t buttonPushed = -1;

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
	uint8_t target[8];        // 6 byte device address (MAC address) or zero (0) means all devices
	uint8_t reserved[6];      // Must all be zero (0)
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
	/* set power */
	uint16_t level;
	uint32_t duration;
} lx_msg_setPower_t;
#pragma pack(pop)

static void print_header(lx_protocol_header_t* head) {
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

static void print_msgStateService(lx_msg_stateService_t* msg) {
	Serial.printf("Service: %d\n", msg->service);
	Serial.printf("Port: %d\n", msg->port);
	Serial.println();
}

static lx_protocol_header_t discovery_header = {
	sizeof(lx_protocol_header_t),
	LIFX_HEADER_PROTOCOL,
	LIFX_HEADER_ADDRESSABLE,                          
	0, // only for discovery header
	LIFX_HEADER_ORIGIN,
	LIFX_HEADER_SOURCE,
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	LIFX_HEADER_RES_REQ,
	LIFX_HEADER_ACK_REQ,
	0,
	LIFX_MSG_GET_SERVICE
};

static lx_protocol_header_t setPower_header = {
	sizeof(lx_protocol_header_t)+sizeof(lx_msg_setPower_t),
	LIFX_HEADER_PROTOCOL,
	LIFX_HEADER_ADDRESSABLE,                          
	0, // 1 for all other messages
	LIFX_HEADER_ORIGIN,
	LIFX_HEADER_SOURCE,
	LIFX_TARGET,
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	LIFX_HEADER_RES_REQ,
	LIFX_HEADER_ACK_REQ,
	0,
	LIFX_MSG_SET_POWER
};

static lx_msg_setPower_t setPower_msg = {
	65535,
	500
};

byte packetBuffer[128];

void isr_p2() {
	buttonPushed = 2;
}

void isr_p0() {
	buttonPushed = 0;
}

void setup() {
	Serial.begin(230400);
	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

	while (WiFi.status() != WL_CONNECTED) {
	#if DEBUG
	delay(500);
	Serial.print(".");
	#endif
	}

	#if DEBUG
	Serial.println();
	Serial.println("WiFi connected");
	Serial.println("IP address: ");
	Serial.println(WiFi.localIP());
	#endif

	bcastAddr = WiFi.localIP();
	bcastAddr[3] = 255;

	UDP.begin(LIFX_UDP_PORT);

	/* Setup Interupts */
	pinMode(2, INPUT_PULLUP);
	attachInterrupt(digitalPinToInterrupt(2), isr_p2, FALLING);
	pinMode(0, INPUT_PULLUP);
	attachInterrupt(digitalPinToInterrupt(0), isr_p0, FALLING);

	Serial.println();
}

static bool turnOn = true;

void loop() {
	interrupts();
	while (buttonPushed==-1) {
		delay(50); // Spinning causes WDT to reset
	}
	noInterrupts();

	Serial.print("Pushed: ");
	Serial.print(buttonPushed, DEC);
	Serial.println();
	Serial.println();

	if (buttonPushed==0) {
		Serial.println("SEND HEADER: GetService");
		print_header(&discovery_header);

		UDP.beginPacket(bcastAddr, LIFX_UDP_PORT);
		char* discoveryHead_ptr = (char*) (&discovery_header);
    	UDP.write(discoveryHead_ptr, sizeof(lx_protocol_header_t));
    	UDP.endPacket();

		delay(500);

		lx_protocol_header_t read_header;
		lx_msg_stateService_t read_msg;

		int packetSize = UDP.parsePacket();
		printf("Packet size: %d\n\n", packetSize);
		UDP.read((char*) (&read_header), sizeof(lx_protocol_header_t));
		UDP.read((char*)(&read_msg), sizeof(lx_msg_stateService_t));
		Serial.println("RECEIVED HEADER");
		print_header(&read_header);
		Serial.println("RECEIVED MESSAGE");
		print_msgStateService(&read_msg);
	}
	else if (buttonPushed==2) {
		Serial.println("SEND HEADER: SetPower");
		print_header(&setPower_header);
		Serial.println("SEND MSG: SetPower");

		if (turnOn) {
			setPower_msg.level = 65535;
		}
		else {
			setPower_msg.level = 65535;
		}
		turnOn = !turnOn;

		UDP.beginPacket(bcastAddr, LIFX_UDP_PORT);
		UDP.write((char*)(&setPower_header), sizeof(lx_protocol_header_t));
		UDP.write((char*)(&setPower_msg), sizeof(lx_msg_setPower_t));
		UDP.endPacket();

	}

	delay(500);
	buttonPushed = -1;
}