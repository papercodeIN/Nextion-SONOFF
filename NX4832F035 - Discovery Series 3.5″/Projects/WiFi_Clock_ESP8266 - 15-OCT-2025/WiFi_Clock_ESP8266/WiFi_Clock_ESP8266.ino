/*
  WiFi_Clock_ESP8266.ino
  -----------------------
  Minimal Wi-Fi + Nextion clock for ESP8266 cores (ESP8266/NodeMCU/WeMos).

  - Displays time on Nextion HMI labels: tTime, tDate, tDay
    * tTime -> HH:MM:SS AM/PM (12-hour clock)
    * tDate -> DD-MMM-YYYY (e.g., 15-Oct-2025)
    * tDay  -> full weekday name (e.g., Wednesday)

  Wiring (Nextion <-> ESP8266):
    Choose pins for Nextion RX/TX. If your board has a spare UART (Serial),
    you can use Serial (TX/RX) or use a SoftwareSerial instance on two GPIOs.
    Example below uses Serial1 (TX only) for Nextion TX (limited on some
    boards) and SoftwareSerial for RX if needed. Adjust as required.

  IMPORTANT: Many Nextion models use 5V TTL on TX/RX. Use a level shifter
  or confirm your model's TTL voltage. ESP8266 GPIOs are NOT 5V tolerant.

  Security: Do NOT hard-code Wi-Fi credentials in source control. Create a
  local `secrets.h` (see `secrets.h.example`) and add it to .gitignore.
*/

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <time.h>
#include <TZ.h>
#include <SoftwareSerial.h>

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

// Choose pins for SoftwareSerial (if you don't want to use the hardware UART)
// Note: SoftwareSerial on ESP8266 only supports certain pins; use carefully.
// RX should be a pin that supports change interrupts. D5/D6 etc usually work.
const uint8_t NEXTION_RX_PIN = D5; // Nextion TX -> ESP8266 D5 (GPIO14)
const uint8_t NEXTION_TX_PIN = D6; // Nextion RX -> ESP8266 D6 (GPIO12)
const unsigned long NEXTION_BAUD = 9600;

SoftwareSerial NextionSerial(NEXTION_RX_PIN, NEXTION_TX_PIN); // RX, TX

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

  // Configure NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Wait for time to be set (simple blocking with timeout)
  time_t now = time(nullptr);
  unsigned long start = millis();
  while (now < 24 * 3600) { // before epoch start
    delay(200);
    now = time(nullptr);
    if (millis() - start > 10000) {
      Serial.println("Failed to obtain time (timeout)");
      return;
    }
  }
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.println("Time synchronized.");
}

// Updates all time/date components on the 'home' page
void updateClockDisplay() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  if (gmtime_r(&now, &timeinfo) == NULL) return;
  // Prepare formatted time/date strings in fixed buffers
  char time_buf[12];  // HH:MM:SS AM/PM (max 11 chars + NUL)
  char date_buf[16]; // DD-MMM-YYYY
  char day_buf[12];  // Full weekday name

  // Convert to localtime using offset
  time_t t = now + gmtOffset_sec; // crude localtime with fixed offset
  struct tm local_tm;
  gmtime_r(&t, &local_tm);
  strftime(time_buf, sizeof(time_buf), "%I:%M:%S %p", &local_tm);
  strftime(date_buf, sizeof(date_buf), "%d-%b-%Y", &local_tm);
  strftime(day_buf, sizeof(day_buf), "%A", &local_tm);

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
    int v = NextionSerial.read();
    if (v >= 0) buf[bufLen++] = (uint8_t)v;
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

  // Initialize Nextion serial (SoftwareSerial) and hardware Serial1 if desired
  NextionSerial.begin(NEXTION_BAUD);
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
