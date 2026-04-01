#include <esp_now.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>

// Servo pins
#define SERVO1_PIN 13
#define SERVO2_PIN 12
#define GAME_WINDOW 1500
#define COUNTDOWN_MS 4000

Servo servo1;
Servo servo2;
WebServer server(80);
WebSocketsServer webSocket(81);

// Game state
bool gameStarted = false;
bool glove1Connected = false;
bool glove2Connected = false;
unsigned long gameStartTime = 0;

// Vote buffers
String votes1[20];
String votes2[20];
int voteCount1 = 0;
int voteCount2 = 0;

// Data packet received from gloves
typedef struct {
  int gloveId;
  char gesture[10];
} GloveData;

void setup() {
  Serial.begin(115200);
  servo1.attach(SERVO1_PIN);
  servo2.attach(SERVO2_PIN);
  servo1.write(0);
  servo2.write(0);
  Serial.println("Server booting...");
}

void loop() {
  
}
```

---

**Commit message:**
```
Initial setup: includes, pin defines, and variable declarations