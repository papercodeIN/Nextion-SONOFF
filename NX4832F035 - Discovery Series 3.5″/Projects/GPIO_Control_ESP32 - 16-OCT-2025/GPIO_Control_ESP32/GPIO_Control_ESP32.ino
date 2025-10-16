// Simplified LED control for ESP32 + Nextion
// - No blink/state-machine. Simple on/off toggle per LED.
// - Respects FORBIDDEN_PIN (won't touch that GPIO).
// - Nextion touch packets (0x65 ...) map component IDs to LED indices via TOGGLE_IDS.
// - Serial commands: led <index> <on|off|toggle>, testpin <gpio>

#define LED_COUNT 3
#include <HardwareSerial.h>
HardwareSerial Nextion(2);
const int NEXTION_RX = 16;
const int NEXTION_TX = 17;
const int ledPins[LED_COUNT] = {18, 19, 21};
const bool ledActiveLow[LED_COUNT] = {false, false, false};
const int FORBIDDEN_PIN = 5; // set to -1 to allow all pins

const byte TOGGLE_IDS[LED_COUNT] = {3, 2, 1}; // map component IDs -> led indices

bool ledOn[LED_COUNT];

void writeLed(int idx, bool on) {
  if (idx < 0 || idx >= LED_COUNT) return;
  int p = ledPins[idx];
  ledOn[idx] = on;
  if (p == FORBIDDEN_PIN) return; // do not touch hardware
  bool level = on ^ ledActiveLow[idx]; // true => HIGH, false => LOW when active-low is false
  digitalWrite(p, level ? HIGH : LOW);
}

void setup() {
  Serial.begin(115200);
  Nextion.begin(9600, SERIAL_8N1, NEXTION_RX, NEXTION_TX);
  for (int i = 0; i < LED_COUNT; ++i) {
    ledOn[i] = false;
    int p = ledPins[i];
    if (p == FORBIDDEN_PIN) {
      Serial.print("DISABLED LED index "); Serial.print(i); Serial.print(" pin "); Serial.println(p);
      continue;
    }
    pinMode(p, OUTPUT);
    // ensure off
    if (ledActiveLow[i]) digitalWrite(p, HIGH); else digitalWrite(p, LOW);
  }
  Serial.println("Simplified LED controller ready");
}

void loop() {
  handleNextionData();
  processSerialCommands();
}

void handleNextionData() {
  while (Nextion.available() >= 7) {
    byte header = Nextion.peek();
    if (header != 0x65) { Nextion.read(); continue; }
    byte packet[7];
    Nextion.readBytes(packet, 7);
    if (packet[4] != 0xFF || packet[5] != 0xFF || packet[6] != 0xFF) continue;
    byte comp = packet[2];
    byte evt = packet[3];
    // find matching toggle id
    for (int i = 0; i < LED_COUNT; ++i) {
      if (comp == TOGGLE_IDS[i] && evt == 1) {
        // toggle logical state; if forbidden, we'll print
        if (ledPins[i] == FORBIDDEN_PIN) {
          Serial.print("Ignored toggle for LED index "); Serial.print(i); Serial.print(" (forbidden pin "); Serial.print(FORBIDDEN_PIN); Serial.println(")");
        } else {
          writeLed(i, !ledOn[i]);
          Serial.print("Button->LED "); Serial.print(i); Serial.print(" now "); Serial.println(ledOn[i] ? "ON" : "OFF");
        }
      }
    }
  }
}

// Simple serial command parsing
void processSerialCommands() {
  static char buf[64]; static size_t idx = 0;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n' || idx >= sizeof(buf)-1) {
      buf[idx] = '\0';
      if (idx > 0) handleSerialCommand(buf);
      idx = 0; continue;
    }
    buf[idx++] = c;
  }
}

void handleSerialCommand(char *line) {
  char copy[64]; strncpy(copy, line, sizeof(copy)); copy[63] = 0;
  char *ctx = NULL;
  char *cmd = strtok_r(copy, " \t", &ctx);
  if (!cmd) return;
  if (strcasecmp(cmd, "led") == 0) {
    char *sidx = strtok_r(NULL, " \t", &ctx);
    char *sact = strtok_r(NULL, " \t", &ctx);
    if (!sidx || !sact) { Serial.println("Usage: led <index> <on|off|toggle>"); return; }
    int idx = atoi(sidx);
    if (idx < 0 || idx >= LED_COUNT) { Serial.println("Index out of range"); return; }
    if (ledPins[idx] == FORBIDDEN_PIN) { Serial.println("Refusing command: forbidden pin"); return; }
    if (strcasecmp(sact, "on") == 0) writeLed(idx, true);
    else if (strcasecmp(sact, "off") == 0) writeLed(idx, false);
    else if (strcasecmp(sact, "toggle") == 0) writeLed(idx, !ledOn[idx]);
    Serial.print("LED "); Serial.print(idx); Serial.print(" -> "); Serial.println(ledOn[idx] ? "ON" : "OFF");
  } else if (strcasecmp(cmd, "testpin") == 0) {
    char *sg = strtok_r(NULL, " \t", &ctx); if (!sg) { Serial.println("Usage: testpin <gpio>"); return; }
    int g = atoi(sg);
    if (g == FORBIDDEN_PIN) { Serial.println("Refusing to test forbidden pin"); return; }
    pinMode(g, OUTPUT); digitalWrite(g, HIGH); delay(250); digitalWrite(g, LOW); Serial.println("Test complete");
  } else {
    Serial.println("Unknown command");
  }
}

