/*
  WiFi_Clock_ESP32.ino
  ---------------------
  Minimal Wi-Fi + Nextion clock for ESP32.

  - Displays time on Nextion HMI labels: tTime, tDate, tDay
    * tTime -> HH:MM:SS AM/PM (12-hour clock)
    * tDate -> DD-MMM-YYYY (e.g., 15-Oct-2025)
    * tDay  -> full weekday name (e.g., Wednesday)

  Wiring (Nextion <-> ESP32):
    Nextion TX -> ESP32 GPIO16 (RX2)
    Nextion RX -> ESP32 GPIO17 (TX2)
    Nextion GND -> ESP32 GND
    Nextion VCC -> appropriate power (check your Nextion model: 5V or 3.3V)

  IMPORTANT: Many Nextion models use 5V TTL on TX/RX. Use a level shifter
  or confirm your model's TTL voltage. ESP32 IO is NOT 5V tolerant.

  Security: Do NOT hard-code Wi-Fi credentials in source control. Create a
  local `secrets.h` (see `secrets.h.example`) and add it to .gitignore.
*/

#include <WiFi.h>
#include "time.h"

// --- Configuration ---
// Provide credentials via a local, untracked `secrets.h` file. If the file
// doesn't exist, the ssid/password will be empty and the device won't attempt
// to connect automatically.
#ifdef USE_SECRETS_H
#include "secrets.h" // should define: const char* ssid; const char* password;
#else
const char* ssid = "Capgemini_4G";     // set via secrets.h or at runtime
const char* password = "MN704116"; // set via secrets.h or at runtime
#endif

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800;    // GMT +5:30 (India Standard Time)
const int daylightOffset_sec = 0;   

// No onboard LED is used in this sanitized build

HardwareSerial NextionSerial(2); // Use Serial2 for Nextion communication (Pins 16/17)
const unsigned long NEXTION_BAUD = 9600;

// --- Nextion Protocol Constants (Decoded from your Hex data) ---
// Headers
const byte NEXTION_TOUCH_EVENT_HEADER = 0x65;

// Page IDs
const byte PAGE_HOME_ID     = 0x00;

// Component IDs (Page 0 - Home)
// COMP_LED_ID removed (no LED on UI)



// timing state used for periodic updates
unsigned long lastSecondMillis = 0;


// Safer Nextion command helper using a stack buffer to avoid dynamic String allocations
#define NEXTION_CMD_BUF 64
void sendNextionCommand(const char* cmd) {
  static const uint8_t term[3] = {0xFF, 0xFF, 0xFF};
  NextionSerial.print(cmd);
  NextionSerial.write(term, sizeof(term));
}

void setNextionText(const char* component, const char* text) {
  char buf[NEXTION_CMD_BUF];
  // Ensure we don't overflow buf. Format: <component>.txt="<text>"
  // snprintf returns the number of bytes that would've been written.
  snprintf(buf, sizeof(buf), "%s.txt=\"%s\"", component, text);
  sendNextionCommand(buf);
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
  // Prepare formatted time/date strings in fixed buffers
  char time_buf[12];  // HH:MM:SS AM/PM (max 11 chars + NUL)
  char date_buf[12]; // DD-MMM-YYYY
  char day_buf[12];  // Full weekday name

  strftime(time_buf, sizeof(time_buf), "%I:%M:%S %p", &timeinfo);
  strftime(date_buf, sizeof(date_buf), "%d-%b-%Y", &timeinfo);
  strftime(day_buf, sizeof(day_buf), "%A", &timeinfo);

  // Send updates only if the value changed since last send (reduces serial traffic)
  static char last_time[12] = "";
  static char last_date[12] = "";
  static char last_day[12] = "";

  if (strncmp(time_buf, last_time, sizeof(time_buf)) != 0) {
    setNextionText("tTime", time_buf);
    strncpy(last_time, time_buf, sizeof(last_time));
  }

  if (strncmp(date_buf, last_date, sizeof(date_buf)) != 0) {
    setNextionText("tDate", date_buf);
    strncpy(last_date, date_buf, sizeof(last_date));
  }

  if (strncmp(day_buf, last_day, sizeof(day_buf)) != 0) {
    setNextionText("tDay", day_buf);
    strncpy(last_day, day_buf, sizeof(last_day));
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

        // --- Handle the event (cleaned) ---
        if (pageId == PAGE_HOME_ID) { // Page 0: Home
          // No interactive components handled on the home page in this build.
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
  // Initialization sequence:
  // 1) Initialize Serial for debug output
  // 2) Start the UART used for Nextion (Serial2) on GPIO16=RX, GPIO17=TX
  // 3) Initialize LED pin
  // 4) Connect to Wi-Fi (uses credentials from secrets.h if provided)
  // 5) Sync time via NTP and perform an initial display update
  // Note: If `secrets.h` is not present, Wi-Fi connect will block until timeout
  Serial.begin(115200);
  delay(1000);

  NextionSerial.begin(NEXTION_BAUD, SERIAL_8N1, 16, 17); 
  delay(100); 

  setupTime();

  updateClockDisplay();
  // updatePomodoroDisplay();

  Serial.println("System Ready.");
}

void loop() {
  unsigned long currentMillis = millis();
  // Main loop responsibilities:
  // 1) Update the Nextion clock display once per second (non-blocking)
  // 2) Poll and handle any incoming Nextion touch events frequently
  if (currentMillis - lastSecondMillis >= 1000) {
    // Update all time/date labels (tTime, tDate, tDay)
    updateClockDisplay();
    lastSecondMillis = currentMillis;
  }

  // Read and process Nextion touch events (non-blocking parser)
  handleNextionEvents();
}
