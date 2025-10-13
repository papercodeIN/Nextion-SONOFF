#include <HardwareSerial.h>

HardwareSerial Nextion(2); // Serial2 on ESP32
const int ledPin = 2;

// LED states
enum LedState {
  LED_OFF,
  LED_GLOW,
  LED_BLINK_SLOW,
  LED_BLINK_FAST
};

LedState currentState = LED_OFF;
unsigned long lastBlinkTime = 0;
const long blinkSlowInterval = 500;
const long blinkFastInterval = 200;

void setup() {
  Serial.begin(115200);
  Nextion.begin(9600, SERIAL_8N1, 16, 17);
  pinMode(ledPin, OUTPUT);
  delay(2000);
  Serial.println("ESP32 Ready for Nextion Touch Events.");
}

void loop() {
  // Handle incoming Nextion data
  handleNextionData();

  // Run LED behavior according to the current state
  switch (currentState) {
    case LED_GLOW:
      digitalWrite(ledPin, HIGH);
      break;

    case LED_BLINK_SLOW:
      blinkLed(blinkSlowInterval);
      break;

    case LED_BLINK_FAST:
      blinkLed(blinkFastInterval);
      break;

    case LED_OFF:
    default:
      digitalWrite(ledPin, LOW);
      break;
  }
}

void handleNextionData() {
  if (Nextion.available() >= 7) {
    byte packet[7];
    Nextion.readBytes(packet, 7);

    // Validate header and terminators
    if (packet[0] != 0x65) return;
    if (packet[4] != 0xFF || packet[5] != 0xFF || packet[6] != 0xFF) return;

    byte componentId = packet[2];
    byte event = packet[3];

    Serial.print("Button ID: ");
    Serial.print(componentId);
    Serial.print("  Event: ");
    Serial.println(event == 1 ? "Pressed" : "Released");

    handleButtonEvent(componentId, event);
  }
}

void handleButtonEvent(byte id, byte event) {
  // Pressed = 1 → start action
  // Released = 0 → stop action (LED_OFF)

  if (event == 1) { // Button pressed
    switch (id) {
      case 2:  // bt0 - Glow
        currentState = LED_GLOW;
        break;

      case 3:  // bt1 - Blink Fast
        currentState = LED_BLINK_FAST;
        break;

      case 4:  // bt2 - Blink Slow
        currentState = LED_BLINK_SLOW;
        break;

      default:
        Serial.println("Unknown button ID");
        break;
    }
  } 
  else if (event == 0) { // Button released
    currentState = LED_OFF;
  }
}

// Non-blocking LED blink
void blinkLed(long interval) {
  if (millis() - lastBlinkTime >= interval) {
    lastBlinkTime = millis();
    digitalWrite(ledPin, !digitalRead(ledPin));
  }
}
