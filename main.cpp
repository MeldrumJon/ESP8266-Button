// Based on https://github.com/aijayadams/esp8266-lifx

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

/*
 * Addresses and Settings
 */

// WiFi Settings
const char *WIFI_SSID = "kcjnet2";
const char *WIFI_PASSWORD = "pokey.bear7";
// LIFX MAC Address
#define LIFX_TARGET {0xD0, 0x73, 0xD5, 0x30, 0x05, 0x45, 0x00, 0x00}
#define LIFX_UDP_PORT 56700

/*
 * WiFi Setup
 */

#define BUFFER_LEN 128
#define TIMEOUT_MS 500

IPAddress bcastAddr(255, 255, 255, 255);
WiFiUDP UDP;
char packetBuffer[128];


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
#define LIFX_TYPE_GETPOWER 20
#define LIFX_TYPE_SETPOWER 21
#define LIFX_TYPE_STATEPOWER 22

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
	0, // Acknowledge Requested
	0, // Sequence
	LIFX_TYPE_SETPOWER // Type (SetPower)
};

static lx_msg_setPower_t setPower_msg = {
	65535 // Level
};

/*
 * LIFX Functions
 */

// void lifx_discover() {
// 	UDP.beginPacket(bcastAddr, LIFX_UDP_PORT);
//     UDP.write( (char*) &discovery_header, sizeof(lx_protocol_header_t));
//     UDP.endPacket();

//   	unsigned long started = millis();
//   	while (millis() - started < TIMEOUT_MS) {
//     	int packetLen = UDP.parsePacket();
//     	if (packetLen && packetLen < BUFFER_LEN) {
//      		UDP.read(packetBuffer, sizeof(packetBuffer));

//       		if (((lx_protocol_header_t*)packetBuffer)->type == LIFX_TYPE_STATEPOWER) {
//         		power = ((lx_msg_statePower_t*)(packetBuffer + sizeof(lx_protocol_header_t)))->level;
//         		return power;
//       		} else {
//         		Serial.print("Unexpected Packet type: ");
//         		Serial.println(((lx_protocol_header_t*)packetBuffer)->type);
//       		}
//     	}
//   	}
// }

uint16_t lifx_getPower() {
  	uint16_t power = 0;
	
  	// Send a packet on startup
  	UDP.beginPacket(bcastAddr, LIFX_UDP_PORT);
  	UDP.write((char *) &getPower_header, sizeof(lx_protocol_header_t));
  	UDP.endPacket();
	
  	unsigned long started = millis();
  	while (millis() - started < TIMEOUT_MS) {
  	  	int packetLen = UDP.parsePacket();
  	  	if (packetLen && packetLen < BUFFER_LEN) {
  	  		int read = UDP.read(packetBuffer, packetLen);
			Serial.printf("PacketLen: %d, read: %d\r\n", packetLen, read);
			lx_protocol_header_t* header = (lx_protocol_header_t*) packetBuffer;
			lx_msg_statePower_t* payload = (lx_msg_statePower_t*) (packetBuffer + sizeof(lx_protocol_header_t));
			print_header(header);
			print_msgStatePower(payload);
  	  		if (header->type == LIFX_TYPE_STATEPOWER) {
  	  	    	power = payload->level;
  	  	    	return power;
  	  	  	} else {
  	  	    	Serial.print("Unexpected Packet type: ");
  	  	    	Serial.println(((lx_protocol_header_t*)packetBuffer)->type);
  	  	  	}
  	  	}
  	}
	
  	return power;
}

/*
 * Button Interrupts
 */
int8_t buttonPushed = -1;

ICACHE_RAM_ATTR void isr_p2() {
	buttonPushed = 2;
}

ICACHE_RAM_ATTR void isr_p0() {
	buttonPushed = 0;
}

/*
 * Wifi Setup
 */

void setup() {
	Serial.begin(115200);
	Serial.println();

	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}

	Serial.println();
	Serial.println("WiFi connected");
	Serial.println("IP address: ");
	Serial.println(WiFi.localIP());

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

void loop() {
	noInterrupts();
	buttonPushed = -1;
	interrupts();
	while (buttonPushed==-1) {
		delay(50); // Spinning causes WDT to reset
	}

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
		uint16_t pow = lifx_getPower();
		Serial.printf("Power level is %d\r\n", pow);
		Serial.println();

		// Serial.println("SEND HEADER: GetPower");
		// print_header(&getPower_header);
		// UDP.beginPacket(bcastAddr, LIFX_UDP_PORT);
		// UDP.write((char*)(&getPower_header), sizeof(lx_protocol_header_t));
		// UDP.endPacket();

		// delay(500);

		// lx_protocol_header_t read_header;
		// lx_msg_statePower_t read_msg;
		// UDP.read((char*) (&read_header), sizeof(lx_protocol_header_t));
		// UDP.read((char*)(&read_msg), sizeof(lx_msg_statePower_t));
		// Serial.println("RECEIVED HEADER");
		// print_header(&read_header);
		// Serial.println("RECEIVED MESSAGE");
		// print_msgStatePower(&read_msg);

		// delay(500);

		// Serial.println("SEND HEADER: SetPower");
		// print_header(&setPower_header);

		// uint16_t level = read_msg.level;
		// if (level) {
		// 	setPower_msg.level = 0;
		// }
		// else {
		// 	setPower_msg.level = 65535;
		// }
		// Serial.println("SEND MSG: SetPower");
		// Serial.print("Level: ");
		// Serial.println(setPower_msg.level, DEC);

		// UDP.beginPacket(bcastAddr, LIFX_UDP_PORT);
		// UDP.write((char*)(&setPower_header), sizeof(lx_protocol_header_t));
		// UDP.write((char*)(&setPower_msg), sizeof(lx_msg_setPower_t));
		// UDP.endPacket();

	}

	delay(500);
}