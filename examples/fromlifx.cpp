#include <SPI.h>        
#include <Ethernet.h>
#include <EthernetUdp.h>

// ---- Network Settings ----

// Our MAC address
uint8_t mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

// Port we listen to
unsigned int localPort = 8888;

// Port for talking to LIFX devices
unsigned int lifxPort  = 56700;

// Remote IP (In this case we broadcast to the entire subnet)
IPAddress broadcast_ip(255, 255, 255, 255);

// --- LIFX Protocol ---

// The LIFX Header structure
#pragma pack(push, 1)
typedef struct {
  /* frame */
  uint16_t size;
  uint16_t protocol:12;
  uint8_t  addressable:1;
  uint8_t  tagged:1;
  uint8_t  origin:2;
  uint32_t source;
  /* frame address */
  uint8_t  target[8];
  uint8_t  reserved[6];
  uint8_t  res_required:1;
  uint8_t  ack_required:1;
  uint8_t  :6;
  uint8_t  sequence;
  /* protocol header */
  uint64_t :64;
  uint16_t type;
  uint16_t :16;
  /* variable length payload follows */
} lifx_header;
#pragma pack(pop)

// Device::SetPower Payload
#pragma pack(push, 1)
typedef struct {
  uint16_t level;
} lifx_payload_device_set_power;
#pragma pack(pop)

// Device::StatePower Payload
#pragma pack(push, 1)
typedef struct {
  uint16_t level;
} lifx_payload_device_state_power;
#pragma pack(pop)

// Payload types
#define LIFX_DEVICE_GETPOWER 20
#define LIFX_DEVICE_SETPOWER 21
#define LIFX_DEVICE_STATEPOWER 22

// Packet buffer size
#define LIFX_INCOMING_PACKET_BUFFER_LEN 300

// Length in bytes of serial numbers
#define LIFX_SERIAL_LEN 6

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

// Timing data
unsigned long sendInterval = 30000; // 30 seconds
unsigned long timeoutInterval = 500;

unsigned long lastSend = 0;

void setup() {
  Serial.begin(19200);
  Serial.print("\n\n\n\n");
  Serial.println("Setting up network");
  Ethernet.begin(mac);  // Start Ethernet, using DHCP
  Udp.begin(localPort); // Listen for incoming UDP packets
  Serial.println("Network is set up");
}

void loop() {
  uint8_t dest[] = {0xd0, 0x73, 0xd5, 0x12, 0x0e, 0x95};
  unsigned long currentTime = millis();
  
  // If 30 seconds have passed, send another packet
  if (currentTime - lastSend >= sendInterval) {
    lastSend = currentTime;
    
    Serial.print("Power level: ");
    Serial.println(GetPower(dest));
  }
  
  Ethernet.maintain();
}

uint16_t GetPower(uint8_t *dest) {
  uint16_t power = 0;
  
  lifx_header header;
  
  // Initialise both structures
  memset(&header, 0, sizeof(header));
  
  // Set the target the nice way
  memcpy(header.target, dest, sizeof(uint8_t) * LIFX_SERIAL_LEN);
  
  // Setup the header
  header.size = sizeof(lifx_header); // Size of header + payload
  header.tagged = 0;
  header.addressable = 1;
  header.protocol = 1024;
  header.source = 123;
  header.ack_required = 0;
  header.res_required = 1;
  header.sequence = 100;
  header.type = LIFX_DEVICE_GETPOWER;
  
  // Send a packet on startup
  Udp.beginPacket(broadcast_ip, 56700);
  Udp.write((char *) &header, sizeof(lifx_header));
  Udp.endPacket();
  
  unsigned long started = millis();
  while (millis() - started < timeoutInterval) {
    int packetLen = Udp.parsePacket();
    byte packetBuffer[LIFX_INCOMING_PACKET_BUFFER_LEN];
    if (packetLen && packetLen < LIFX_INCOMING_PACKET_BUFFER_LEN) {
      Udp.read(packetBuffer, sizeof(packetBuffer));
      
      if (((lifx_header *)packetBuffer)->type == LIFX_DEVICE_STATEPOWER) {
        power = ((lifx_payload_device_state_power *)(packetBuffer + sizeof(lifx_header)))->level;
        return power;
      } else {
        Serial.print("Unexpected Packet type: ");
        Serial.println(((lifx_header *)packetBuffer)->type);
      }
    }
  }
  
  return power;
}