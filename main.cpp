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

#define LIFX_UDP_PORT 56700 // Can be changed for newer bulbs, best compatibility use 56700

const char *WIFI_SSID = "125 2.4GHz";
const char *WIFI_PASSWORD = "fivetimesfive";

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

// lx_protocol_header_t testHeader = {
//   .size = 36,
//   .protocol = 1024,
//   .addressable = 1,
//   .tagged = 
// };

#pragma pack(push, 1)
typedef struct {
	/* set power */
	uint16_t level;
	uint32_t duration;
} lx_set_power_t;
#pragma pack(pop)


byte packetBuffer[128];

// void lxDiscovery() {
// 	/* Build lxDiscovery payload */
// 	byte target_addr[8] = {0};
// 	lx_protocol_header_t *lxHead;
// 	lxHead = (lx_protocol_header_t *)calloc(1, sizeof(lx_protocol_header_t));
// 	lxMakeFrame(lxHead, 0, 1, target_addr, 2);

// 	/* Start listening for responses */
// 	UDP.begin(4097);
// 	delay(500);
// 	/* Send a couple of discovery packets out to the network*/
// 	for (int i = 0; i < 1; i++) {
// 	byte *b = (byte *)lxHead;
// 	UDP.beginPacket(bcastAddr, lxPort);
// 	UDP.write(b, sizeof(lx_protocol_header_t));
// 	UDP.endPacket();

// 	delay(500);
// 	}

// 	free(lxHead);

// 	for (int j = 0; j < 20; j++) {

// 	int sizePacket = UDP.parsePacket();
// 	if (sizePacket) {
// 		UDP.read(packetBuffer, sizePacket);
// 		byte target_addr[SIZE_OF_MAC];
// 		memcpy(target_addr, packetBuffer + 8, SIZE_OF_MAC);
// 		// Print MAC
// 		for (int i = 0; i < sizeof(target_addr); i++) {
// 		Serial.print(target_addr[i - 1], HEX);
// 		Serial.print(" ");
// 		}
// 		Serial.println();

// 		// Check if this device is new
// 		uint8_t previouslyKnownDevice = 0;
// 		for (int i = 0; i < LX_DEVICES; i++) {
// 		if (!memcmp(lxDevices[i], target_addr, SIZE_OF_MAC)) {
// 			Serial.println("Previously seen target");
// 			previouslyKnownDevice = 1;
// 			break;
// 		}
// 		}
// 		// Store new devices
// 		if (!previouslyKnownDevice) {
// 		lxDevicesAddr[LX_DEVICES] = (uint32_t)UDP.remoteIP();

// 		Serial.println(UDP.remoteIP());
// 		memcpy(lxDevices[LX_DEVICES++], target_addr, SIZE_OF_MAC);
// 		Serial.print("Storing device as LX_DEVICE ");
// 		Serial.println(LX_DEVICES);
// 		}
// 	}
// 	delay(20);
// 	}
// }

// void lxPower(uint16_t level) {
//   for (int i = 0; i < LX_DEVICES; i++) {
//     /* Set target_addr from know lxDevices */
//     byte target_addr[8] = {0};
//     memcpy(target_addr, lxDevices[i], SIZE_OF_MAC);

//     /* Build payload */
//     uint8_t type = 117;
//     uint32_t duration = 500;

//     lx_protocol_header_t *lxHead;
//     lxHead = (lx_protocol_header_t *)calloc(1, sizeof(lx_protocol_header_t));
//     lxMakeFrame(lxHead, sizeof(lx_set_power_t), 0, target_addr, type);

//     lx_set_power_t *lxSetPower;
//     lxSetPower = (lx_set_power_t *)calloc(1, sizeof(lx_set_power_t));
//     lxSetPower->duration = 500;
//     lxSetPower->level = level;

//     UDP.beginPacket(lxDevicesAddr[i], lxPort);
//     byte *b = (byte *)lxHead;
//     UDP.write(b, sizeof(lx_protocol_header_t));
//     b = (byte *)lxSetPower;
//     UDP.write(b, sizeof(lx_set_power_t));
//     UDP.endPacket();

//     free(lxSetPower);
//     free(lxHead);

//     Serial.println(lxDevicesAddr[i]);
//   }
//   Serial.println();
// }

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

lx_protocol_header_t discovery_header = {
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

lx_protocol_header_t read_header;

static void printHeader(lx_protocol_header_t* head) {
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
		Serial.println("SEND HEADER");
		printHeader(&discovery_header);

		UDP.beginPacket(bcastAddr, LIFX_UDP_PORT);
		char* discoveryHead_ptr = (char*) (&discovery_header);
    	UDP.write(discoveryHead_ptr, sizeof(lx_protocol_header_t));
    	UDP.endPacket();

		delay(500);

		int packetSize = UDP.parsePacket();
		char* readHead_ptr = (char*) (&read_header);
		int readSize = (packetSize < sizeof(lx_protocol_header_t)) ? packetSize : sizeof(lx_protocol_header_t);
		UDP.read(readHead_ptr, readSize);

		Serial.println("RECEIVE HEADER");
		printHeader(&read_header);
	}
	else if (buttonPushed==2) {
		Serial.println("TWO!");
	}

	delay(500);
	buttonPushed = -1;
}