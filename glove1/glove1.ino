#include <esp_now.h>
#include <WiFi.h>

// Glove identity and pin setup
#define GLOVE_ID      1
#define FLEX_INDEX    34
#define FLEX_RING     35
#define THRESHOLD     3800

// Server MAC address
uint8_t serverMac[] = {0x88, 0x57, 0x21, 0x2E, 0xB0, 0x38};

// Data packet we send to server
typedef struct {
  int gloveId;
  char gesture[10];
} GloveData;

GloveData data;

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.setChannel(1);
  Serial.println("Glove booting...");
}

void loop() {
  
  delay(100);
}