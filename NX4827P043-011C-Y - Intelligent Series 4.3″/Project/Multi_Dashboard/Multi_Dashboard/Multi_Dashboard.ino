#include <Arduino.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <DHT.h>
#include <WebServer.h>

const char* WIFI_SSID = "Bhavya";
const char* WIFI_PASSWORD = "bhaVYA@02";
const String presetPassword = "142356"; // 6-digit password

WebServer server(80);

// WiFi & Sensor Data
bool wifiConnected = false;
int temperature = 0;
int humidity = 0;

// --- HARDWARE PINS ---
#define NEXTION_RX_PIN 16 // Connects to Nextion TX
#define NEXTION_TX_PIN 17 // Connects to Nextion RX
#define DHT_PIN 4         // DHT11 Data Pin
#define DHT_TYPE DHT11

// --- GLOBAL VARIABLES & STATE ---
// Timers for non-blocking updates
unsigned long lastTimeUpdate = 0;
unsigned long lastSensorUpdate = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long ignoreUntil = 0; // For debouncing Nextion inputs

// --- NEXTION & TIME ---
HardwareSerial nextion(2); // Use UART2 for Nextion
const char* NTP_SERVER = "pool.ntp.org";
const long GMT_OFFSET_SEC = 19800; // India Standard Time (UTC +5:30)
const int DAYLIGHT_OFFSET_SEC = 0;


// Password
String tPassword = "";
const unsigned long IGNORE_MS_AFTER_CLEAR = 300;

void clearPasswordFieldAndFlush() {
  tPassword = "";
  sendToNextion("p2_pass.tPassword.txt=\"\"");
  Serial.println("Password cleared (ESP + Nextion)");
  while (nextion.available()) {
    nextion.read();
  }
  ignoreUntil = millis() + IGNORE_MS_AFTER_CLEAR;
}

inline bool isIgnoring() {
  return millis() < ignoreUntil;
}


WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_SERVER, GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC);
DHT dht(DHT_PIN, DHT_TYPE);


// Alarm
int alarmHour = 7;
int actualHour = 7;
int alarmMinute = 30;
bool alarmIsPM = false; // false = AM, true = PM
bool alarmEnabled = false;
bool alarmDays[7] = {false, false, false, false, false, false, false}; // Sun, Mon, Tue, Wed, Thu, Fri, Sat
bool alarmSounding = false;
unsigned long alarmStopTime = 0;
String alarmTime = "7:30";

void initializeAlarm() {
    alarmHour = 7;
    alarmMinute = 30;
    alarmIsPM = false; // AM
    alarmEnabled = true; // Alarm system on by default
    for(int i=0; i<7; i++) { alarmDays[i] = false; } // All days off by default
}

void checkAlarm() {
  uint8_t buffer[20];
  int idx = 0;
  unsigned long start = millis();

  while (idx < sizeof(buffer)) {
    if (nextion.available()) {
      uint8_t b = nextion.read();
      if (b == 'a') break;  // end marker
      buffer[idx++] = b;
    }
  }

  if (idx < 16) return;  // not enough data received

  alarmHour = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);
  alarmMinute = buffer[4] | (buffer[5] << 8) | (buffer[6] << 16) | (buffer[7] << 24);
  alarmIsPM = buffer[8] | (buffer[9] << 8) | (buffer[10] << 16) | (buffer[11] << 24);
  alarmEnabled = buffer[12] | (buffer[13] << 8) | (buffer[14] << 16) | (buffer[15] << 24);

  int actualHour = (alarmIsPM == 1) ? (alarmHour + 12) % 24 : alarmHour;
  alarmTime = String(actualHour) + ":" + (alarmMinute < 10 ? "0" : "") + String(alarmMinute);

  Serial.println("Alarm Time: " + alarmTime);
  Serial.println(alarmEnabled ? "Enabled" : "Disabled");
  alarmSounding = alarmEnabled;
}

void checkAlarmTrigger() {
  if (!alarmEnabled) return; // skip if alarm is disabled

  // Get current hour/minute in 24-hour format
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();

  int currentHour = hour(epochTime);
  int currentMinute = minute(epochTime);

  // Convert alarm to 24-hour format for correct comparison
  int alarmHour24 = (alarmIsPM) ? (alarmHour % 12) + 12 : (alarmHour % 12);

  // Compare
  if (currentHour == alarmHour24 && currentMinute == alarmMinute) {
    if (alarmSounding) {
      // --- ALARM TRIGGER ---
      Serial.println("⏰ Alarm triggered!");
      sendToNextion("tmrAlarm1.en=1");
      sendToNextion("tmrAlarm2.en=1");
      sendToNextion("tmrAlarm3.en=1");
      alarmSounding = false;
    }
  }
}



// Wifi
void setupWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  
  // Update Nextion display status
  sendToNextion("p1_lock.tLockStatus.txt=\"Connecting...\"");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 20) {
    delay(500);
    Serial.print(".");
    attempt++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    wifiConnected = true;
    sendToNextion("p1_lock.tLockStatus.txt=\"Connected\"");
  } else {
    Serial.println("\nFailed to connect to WiFi.");
    wifiConnected = false;
    sendToNextion("p1_lock.tLockStatus.txt=\"Offline\"");
  }
}



// WebServer setup
void handleRoot() {
  // This is the HTML for the webpage. R"rawliteral(...)rawliteral" is a way to write multi-line strings.
  const char* html = R"rawliteral(
  <!DOCTYPE HTML><html><head>
    <title>ESP32 Reset</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
      body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; background-color: #2c3e50; color: #ecf0f1; }
      .btn { background-color: #e74c3c; border: none; color: white; padding: 15px 32px;
             text-align: center; text-decoration: none; display: inline-block;
             font-size: 16px; margin: 4px 2px; cursor: pointer; border-radius: 8px; }
    </style>
    </head><body>
    <h2>ESP32 Remote Reset</h2>
    <p>Click the button below to restart the device.</p>
    <form action="/reset" method="POST">
      <input type="submit" class="btn" value="Reset ESP32">
    </form>
  </body></html>
  )rawliteral";
  server.send(200, "text/html", html);
}

void handleReset() {
  server.send(200, "text/plain", "ESP32 is restarting...");
  delay(200); // Give the server a moment to send the response
  ESP.restart();
}



// Time
// Formatted Time Strings
String lockTime = "00:00";
String lockMeridian = "AM";
String lockDayDate = "---, --- 00";
String homeTime = "00:00";
String homeMeridian = "AM";
String homeDate = "00-00";
String homeDay = "---";
char timeBuffer[20];

void updateTime() {
  if (!wifiConnected) return; // Can't get time without WiFi

  char dayTemp[20];
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();
  
  // Format time for lock screen (e.g., 10:30, PM, Tue, Oct 14)
  snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d", hourFormat12(epochTime), minute(epochTime));
  lockTime = timeBuffer;
  lockMeridian = (isAM(epochTime)) ? "AM" : "PM";
  snprintf(dayTemp, sizeof(dayTemp), "%.3s", dayShortStr(weekday(epochTime)));
  snprintf(timeBuffer, sizeof(timeBuffer), ", %.3s %d", monthShortStr(month(epochTime)), day(epochTime));
  lockDayDate = dayTemp;
  lockDayDate += timeBuffer;

  // Format time for home screen (e.g., 10, 30, PM, 14-10, Tue)
  snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d", hourFormat12(epochTime), minute(epochTime));
  homeTime = timeBuffer;
  homeMeridian = lockMeridian;
  snprintf(timeBuffer, sizeof(timeBuffer), "%02d-%02d", day(epochTime), month(epochTime));
  homeDate = timeBuffer;
  snprintf(timeBuffer, sizeof(timeBuffer), "%.3s", dayShortStr(weekday(epochTime)));
  homeDay = timeBuffer;
}


void readSensors() {
  float h = dht.readHumidity();
  float t = dht.readTemperature(); // Read in Celsius

  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }
  
  temperature = (int)t;
  humidity = (int)h;
}


void updateVisiblePage() {
    updateLockScreen();
    updateHomeScreen();
}


void updateLockScreen() {
    sendToNextion("p1_lock.tLockTime.txt=\"" + lockTime + "\"");
    sendToNextion("p1_lock.tLockMeridian.txt=\"" + lockMeridian + "\"");
    sendToNextion("p1_lock.tLockDayDate.txt=\"" + lockDayDate + "\"");
    sendToNextion("p1_lock.tLockTemp.txt=\"" + String(temperature) + "\"");
    sendToNextion("p1_lock.tLockStatus.txt=\"Connected\"");
    sendToNextion("p1_lock.tLockAlarm.txt=\"" + alarmTime + "\"");
}


void updateHomeScreen() {
    sendToNextion("p3_home.tHomeTime.txt=\"" + homeTime + "\"");
    sendToNextion("p3_home.tHomeMeridian.txt=\"" + homeMeridian + "\"");
    sendToNextion("p3_home.tHomeDate.txt=\"" + homeDate + "\"");
    sendToNextion("p3_home.tHomeDay.txt=\"" + homeDay + "\"");

    // Update temperature and its progress bar (jTemp)
    sendToNextion("p3_home.tTemp.txt=\"" + String(temperature) + "\"");
    // Map temperature (0-50°C) to progress bar value (0-100)
    int tempBarValue = map(temperature, 0, 50, 0, 100);
    sendToNextion("p3_home.jTemp.val=" + String(tempBarValue));

    // Update humidity and its progress bar (jHum)
    sendToNextion("p3_home.tHum.txt=\"" + String(humidity) + "\"");
    sendToNextion("p3_home.jHum.val=" + String(humidity)); // Humidity is already 0-100
}

void sendToNextion(const String &cmd) {
  nextion.print(cmd);
  nextion.write(0xFF);
  nextion.write(0xFF);
  nextion.write(0xFF);
}


String getTextFromNextion(const String &component) {
  // 1️⃣ Send request
  sendToNextion("get " + component + ".txt");

  // 2️⃣ Wait for reply
  unsigned long start = millis();
  while (!nextion.available()) {
    if (millis() - start > 500) return ""; // timeout
  }

  // 3️⃣ Read response
  String result = "";
  if (nextion.read() == 0x70) { // text return header
    while (true) {
      if (nextion.available()) {
        char c = nextion.read();
        if (c == 0xFF) {
          // Reached the first 0xFF terminator — ignore the next two and break
          while (nextion.available()) nextion.read();
          break;
        }
        result += c;
      }
    }
  }
  return result;
}


// Main Listening code
void listenToNextion() {
  if (!nextion.available()) return;

  while (nextion.available()) {
    char c = nextion.read();

    // Ignore Nextion terminators immediately
    if (c == 0xFF) continue;

    // If we're in the ignore window just drop everything
    if (isIgnoring()) {
      // optionally consume terminators that might arrive after flush
      continue;
    }

    switch (c) {
      case 'O':  // OK pressed
        tPassword = getTextFromNextion("tPassword");
        Serial.println("Display password: " + tPassword);
        Serial.println("OK pressed, verifying...");
        if (tPassword == presetPassword) {
          Serial.println("✅ Password correct!");
          // go to home page
          sendToNextion("page p3_home");
          // clear and flush, then ignore brief interval
          clearPasswordFieldAndFlush();
        } else {
          Serial.println("❌ Wrong password!");
          // show the incorrect message (user replaced page change with this)
          sendToNextion("tPassIncorrect.txt=\"Password Incorrect!\"");
          // clear the field and flush incoming bytes to avoid ghost input
          clearPasswordFieldAndFlush();
        }
        break;
      
      case 'A':
        Serial.println("Alarm System Page functions activated.");
        checkAlarm();

      default:
        break;
    }
  }
}


void setup() {
  // Start serial for debugging
  Serial.begin(115200);

  // Start serial for Nextion display
  // Using pins defined in config.h (GPIO 16 for RX, 17 for TX)
  nextion.begin(57600, SERIAL_8N1, NEXTION_RX_PIN, NEXTION_TX_PIN);

  delay(2000);
  Serial.println("\n--- Nextion Dashboard Initializing ---");
  sendToNextion("rest");

  // Initialize DHT11 sensor
  dht.begin();
  Serial.println("DHT11 Sensor Initialized.");

  // Initialize alarm settings to default values
  initializeAlarm();
  Serial.println("Default alarm values set.");

  // Connect to WiFi
  setupWiFi();

  // If WiFi is connected, initialize NTP client for time
  if (WiFi.status() == WL_CONNECTED) {
    timeClient.begin();
    Serial.println("NTP Client started.");

    server.on("/", HTTP_GET, handleRoot);     // When someone visits the IP, show the main page
    server.on("/reset", HTTP_POST, handleReset); // When the button is pressed, call the reset handler
    server.begin();                           // Start the web server
    Serial.println("Web Server for remote reset is running.");
  } else {
    Serial.println("WiFi not connected. Time will not be synced.");
  }

  Serial.println("--- Initialization Complete. Running. ---");
}


void loop() {
  listenToNextion();

  if (millis() - lastTimeUpdate >= 1000) {
    updateTime();
    lastTimeUpdate = millis();
  }

  if (millis() - lastSensorUpdate >= 5000) {
    readSensors();
    lastSensorUpdate = millis();
  }

  if (millis() - lastDisplayUpdate >= 1000) {
    updateVisiblePage();
    lastDisplayUpdate = millis();
  }

  checkAlarmTrigger();

  server.handleClient();
}