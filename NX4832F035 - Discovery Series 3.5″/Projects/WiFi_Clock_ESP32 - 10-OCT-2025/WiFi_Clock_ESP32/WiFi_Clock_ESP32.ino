#include <WiFi.h>
#include "time.h"

// --- Configuration ---
const char* ssid = "Bhavya";         // REPLACE with your Wi-Fi name
const char* password = "bhaVYA@02"; // REPLACE with your Wi-Fi password

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800;    // GMT +5:30 (India Standard Time)
const int daylightOffset_sec = 0;   

// ESP32 GPIO for the built-in LED (often pin 2)
const int LED_PIN = 2; 

HardwareSerial NextionSerial(2); // Use Serial2 for Nextion communication (Pins 16/17)
const unsigned long NEXTION_BAUD = 9600;

// --- Nextion Protocol Constants (Decoded from your Hex data) ---
// Headers
const byte NEXTION_TOUCH_EVENT_HEADER = 0x65;

// Page IDs
const byte PAGE_HOME_ID     = 0x00;
const byte PAGE_POMODORO_ID = 0x01;

// Component IDs (Page 0 - Home)
const byte COMP_LED_ID  = 0x06; // bLed
const byte COMP_POM_ID  = 0x07; // bPom

// Component IDs (Page 1 - Pomodoro)
const byte COMP_HOME_ID   = 0x03; // bHome
const byte COMP_INC_ID    = 0x04; // bInc
const byte COMP_DCR_ID    = 0x05; // bDcr
const byte COMP_START_ID  = 0x06; // bStart
const byte COMP_STOP_ID   = 0x07; // bStop
const byte COMP_RESET_ID  = 0x08; // bReset

enum PomodoroState { IDLE, RUNNING, PAUSED, ALERT };
PomodoroState pomodoroState = IDLE;

long pomodoroTimeSeconds = 1500; // Default 25 minutes (25 * 60)
unsigned long lastSecondMillis = 0;
unsigned long timerStartMillis = 0;
long timeRemaining = 0;

unsigned long ledStartTime = 0;
unsigned long ledBlinkDuration = 0; 
unsigned long ledLastToggle = 0;
int ledBlinkSpeed = 0; 
bool ledState = false;

void sendNextionCommand(const String& command) {
  NextionSerial.print(command);
  NextionSerial.write(0xFF);
  NextionSerial.write(0xFF);
  NextionSerial.write(0xFF);
}

void setNextionText(const String& component, const String& text) {
  sendNextionCommand(component + ".txt=\"" + text + "\"");
}


void setupTime() {
  Serial.print("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected.");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println("Time synchronized.");
}

// Updates all time/date components on the 'home' page
void updateClockDisplay() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  // Time (tTime, tSec)
  char time_buf[6]; 
  char sec_buf[3];  
  strftime(time_buf, sizeof(time_buf), "%H:%M", &timeinfo);
  strftime(sec_buf, sizeof(sec_buf), "%S", &timeinfo);
  setNextionText("tTime", String(time_buf));
  setNextionText("tSec", String(sec_buf));

  // Date (tDate)
  char date_buf[6]; 
  strftime(date_buf, sizeof(date_buf), "%d-%m", &timeinfo);
  setNextionText("tDate", String(date_buf));
  
  // Day (tDay - 3 letters)
  char day_buf[4]; 
  strftime(day_buf, sizeof(day_buf), "%a", &timeinfo);
  setNextionText("tDay", String(day_buf));
}

// --- Pomodoro Logic (Non-blocking) ---

String formatTime(long seconds) {
  int minutes = seconds / 60;
  int secs = seconds % 60;
  char buf[6];
  sprintf(buf, "%02d:%02d", minutes, secs);
  return String(buf);
}

void updatePomodoroDisplay() {
  setNextionText("tTimer", formatTime(pomodoroTimeSeconds));
}

void startPomodoro() {
  if (pomodoroState == IDLE || pomodoroState == PAUSED) {
    pomodoroState = RUNNING;
    timerStartMillis = millis();
    timeRemaining = pomodoroTimeSeconds;
    Serial.println("Pomodoro Started/Resumed.");
  }
}

void stopPomodoro() {
  if (pomodoroState == RUNNING) {
    pomodoroState = PAUSED;
    // Calculate and save the remaining time when pausing
    long elapsed = (millis() - timerStartMillis) / 1000;
    pomodoroTimeSeconds = timeRemaining - elapsed;
    updatePomodoroDisplay();
    Serial.println("Pomodoro Paused.");
  }
}

void resetPomodoro() {
  pomodoroState = IDLE;
  pomodoroTimeSeconds = 1500; // Reset to 25 minutes
  updatePomodoroDisplay();
  Serial.println("Pomodoro Reset.");
}

void incrementTimer() {
  if (pomodoroState == IDLE) {
    pomodoroTimeSeconds = min(pomodoroTimeSeconds + 60, 3600L); // Max 60 min
    updatePomodoroDisplay();
  }
}

void decrementTimer() {
  if (pomodoroState == IDLE) {
    pomodoroTimeSeconds = max(pomodoroTimeSeconds - 60, 60L); // Min 1 min
    updatePomodoroDisplay();
  }
}

// Manages the countdown logic
void runPomodoro() {
  if (pomodoroState == RUNNING) {
    unsigned long elapsed = (millis() - timerStartMillis) / 1000;
    long currentRemaining = timeRemaining - elapsed;

    if (currentRemaining > 0) {
      if (currentRemaining != pomodoroTimeSeconds) {
        pomodoroTimeSeconds = currentRemaining;
        updatePomodoroDisplay();
      }
    } else {
      // Timer finished!
      pomodoroState = ALERT;
      pomodoroTimeSeconds = 0;
      updatePomodoroDisplay();
      
      // NON-BLOCKING LED ALERT: 10 seconds at 200ms speed
      startLedAlert(10000, 200); 
      Serial.println("Pomodoro Finished! Starting non-blocking LED alert.");
    }
  }
}

// --- LED Alert Functions (Non-blocking) ---

// Starts a non-blocking LED alert
void startLedAlert(unsigned long duration, int speed) {
  ledStartTime = millis();
  ledBlinkDuration = duration;
  ledBlinkSpeed = speed;
  ledLastToggle = 0; 
  ledState = false;
  pinMode(LED_PIN, OUTPUT);
  // Ensure LED is off before the first toggle
  digitalWrite(LED_PIN, LOW);
}

// Runs the LED logic (must be called frequently in loop())
void runLedAlert() {
  if (ledBlinkDuration > 0) {
    // Check if the total alert duration has passed
    if (millis() - ledStartTime < ledBlinkDuration) {
      // Check if it's time to toggle the LED
      if (millis() - ledLastToggle >= ledBlinkSpeed) {
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState);
        ledLastToggle = millis();
      }
    } else {
      // Alert finished, turn off LED and reset state
      digitalWrite(LED_PIN, LOW);
      ledBlinkDuration = 0;
      Serial.println("LED Alert Finished.");
      pomodoroState = IDLE; // Back to IDLE after alert
    }
  }
}

// ------------------------
// Robust Nextion parser
// ------------------------

#define NEXTION_PACKET_LEN 7   // 65, page, comp, evt, FF, FF, FF
#define NEXTION_BUF_CAP 128

void handleNextionEvents() {
  static uint8_t buf[NEXTION_BUF_CAP];
  static int bufLen = 0;

  // Read all available bytes into buffer (without blocking)
  while (NextionSerial.available() > 0 && bufLen < NEXTION_BUF_CAP) {
    buf[bufLen++] = NextionSerial.read();
  }

  // Parse buffer for complete packets (0x65 ... 0xFF 0xFF 0xFF)
  int i = 0;
  while (i <= bufLen - NEXTION_PACKET_LEN) {
    // look for header byte
    if (buf[i] == NEXTION_TOUCH_EVENT_HEADER) {
      // check termination bytes (safety: ensure we have enough bytes)
      if (buf[i + 4] == 0xFF && buf[i + 5] == 0xFF && buf[i + 6] == 0xFF) {
        // We have a full valid packet at offset i
        uint8_t pageId = buf[i + 1];
        uint8_t componentId = buf[i + 2];
        // uint8_t eventType = buf[i + 3]; // available if needed

        Serial.printf("Parsed Nextion event: page=%d comp=%d\n", pageId, componentId);

        // --- Handle the event (exact same logic as before) ---
        if (pageId == PAGE_HOME_ID) { // Page 0: Home
          if (componentId == COMP_POM_ID) {
            sendNextionCommand("page pomodoro");
            updatePomodoroDisplay();
          } else if (componentId == COMP_LED_ID) {
            // Non-blocking LED test 3s/50ms
            startLedAlert(3000, 50);
          }
        } else if (pageId == PAGE_POMODORO_ID) { // Page 1: Pomodoro
          if (componentId == COMP_HOME_ID) {
            sendNextionCommand("page home");
          } else if (componentId == COMP_INC_ID) {
            incrementTimer();
          } else if (componentId == COMP_DCR_ID) {
            decrementTimer();
          } else if (componentId == COMP_START_ID) {
            startPomodoro();
          } else if (componentId == COMP_STOP_ID) {
            stopPomodoro();
          } else if (componentId == COMP_RESET_ID) {
            resetPomodoro();
          }
        }

        // remove consumed bytes (packet at i of length NEXTION_PACKET_LEN)
        int remaining = bufLen - (i + NEXTION_PACKET_LEN);
        if (remaining > 0) {
          memmove(buf, buf + i + NEXTION_PACKET_LEN, remaining);
        }
        bufLen = remaining;
        i = 0; // restart scanning from buffer start
        continue;
      } else {
        // header found but termination not yet in buffer -> wait for more bytes
        break;
      }
    } else {
      // not header: drop this single byte and continue (it might be 0x04 or garbage)
      i++;
    }
  }
  
  if (i > 0 && i < bufLen) {
    int remaining = bufLen - i;
    memmove(buf, buf + i, remaining);
    bufLen = remaining;
  } else if (i == bufLen) {
    bufLen = 0;
  }

  if (bufLen > NEXTION_BUF_CAP - 8) {
    bufLen = 0;
  }
}


void setup() {
  Serial.begin(115200);
  delay(1000);

  NextionSerial.begin(NEXTION_BAUD, SERIAL_8N1, 16, 17); 
  delay(100); 

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  setupTime();

  updateClockDisplay();
  updatePomodoroDisplay();

  Serial.println("System Ready.");
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastSecondMillis >= 1000) {
    updateClockDisplay();
    lastSecondMillis = currentMillis;
  }

  runPomodoro();

  runLedAlert();

  handleNextionEvents();
}
