#include <esp_now.h>
#include <WiFi.h>

#define GLOVE_ID      2        
#define FLEX_INDEX    35       // change to 32 for glove2
#define FLEX_RING     34       // change to 33 for glove2
#define THRESHOLD_INDEX  3350
#define THRESHOLD_RING   3450

uint8_t serverMac[] = {0x88, 0x57, 0x21, 0x2E, 0xB0, 0x38};

typedef struct {
  int gloveId;
  char gesture[10];
} GloveData;

GloveData data;

String detectGesture() {
  long idxSum = 0, ringSum = 0;
  for (int i = 0; i < 5; i++) {
    idxSum  += analogRead(FLEX_INDEX);
    ringSum += analogRead(FLEX_RING);
    delay(10);
  }

  bool indexBent = (idxSum / 5) > THRESHOLD_INDEX;
  bool ringBent  = (ringSum / 5) > THRESHOLD_RING;

  Serial.print("INDEX avg: "); Serial.print(idxSum / 5);
  Serial.print(" | RING avg: "); Serial.print(ringSum / 5);
  Serial.print(" | indexBent: "); Serial.print(indexBent);
  Serial.print(" | ringBent: "); Serial.print(ringBent);
  Serial.print(" | Gesture: ");

  if (indexBent && ringBent)   return "rock";
  if (!indexBent && !ringBent) return "paper";
  if (!indexBent && ringBent)  return "scissors";
  return "unknown";  
}

void onSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  Serial.print("Send: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.setChannel(1);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  esp_now_register_send_cb(onSent);

  esp_now_peer_info_t peer;
  memset(&peer, 0, sizeof(peer));
  memcpy(peer.peer_addr, serverMac, 6);
  peer.channel = 1;
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  Serial.println("Glove ready!");
}

void loop() {
  String gesture = detectGesture();
  data.gloveId = GLOVE_ID;
  strcpy(data.gesture, gesture.c_str());
  esp_now_send(serverMac, (uint8_t *)&data, sizeof(data));

  Serial.print("INDEX: "); Serial.print(analogRead(FLEX_INDEX));
  Serial.print(" | RING: "); Serial.print(analogRead(FLEX_RING));
  Serial.print(" | Gesture: "); Serial.println(gesture);

  delay(100);
}