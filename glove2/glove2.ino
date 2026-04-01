#include <esp_now.h>
#include <WiFi.h>

// Same as glove 1 but different ID and pins
#define GLOVE_ID      2
#define FLEX_INDEX    32
#define FLEX_RING     33
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