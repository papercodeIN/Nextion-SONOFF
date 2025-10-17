/**
 * @file config.h
 * @author Gemini
 * @brief Configuration, variables, pin definitions, and helper functions.
 * * This file centralizes all the user-configurable settings and the logic for 
 * handling specific pages like the password screen, settings, and alarm.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <DHT.h>

// --- USER CONFIGURATION ---
const char* WIFI_SSID = "Bhavya";
const char* WIFI_PASSWORD = "bhaVYA@02";
const String presetPassword = "142356"; // 6-digit password

// --- HARDWARE PINS ---
#define NEXTION_RX_PIN 16 // Connects to Nextion TX
#define NEXTION_TX_PIN 17 // Connects to Nextion RX
#define DHT_PIN 4         // DHT11 Data Pin
#define DHT_TYPE DHT11

// --- NEXTION & TIME ---
HardwareSerial nextion(2); // Use UART2 for Nextion
const char* NTP_SERVER = "pool.ntp.org";
const long GMT_OFFSET_SEC = 19800; // India Standard Time (UTC +5:30)
const int DAYLIGHT_OFFSET_SEC = 0;

// --- STATE MACHINE ---
// This enum tracks which page is currently active on the Nextion display.
enum PageState {
  PAGE_UNKNOWN,
  PAGE_PASSWORD,
  PAGE_SETTINGS,
  PAGE_ALARM
};
PageState currentPage = PAGE_UNKNOWN; // Start in an unknown state.

// --- GLOBAL VARIABLES & STATE ---
// Timers for non-blocking updates
unsigned long lastTimeUpdate = 0;
unsigned long lastSensorUpdate = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long ignoreUntil = 0; // For debouncing Nextion inputs

// WiFi & Sensor Data
bool wifiConnected = false;
int temperature = 0;
int humidity = 0;

// Formatted Time Strings
String lockTime = "00:00";
String lockMeridian = "AM";
String lockDayDate = "---, --- 00";
String homeTime = "00:00";
String homeMeridian = "AM";
String homeDate = "00-00";
String homeDay = "---";
char timeBuffer[20];

// Password Page State
String tPassword = ""; // No longer used to build the password, but kept for consistency
const unsigned long IGNORE_MS_AFTER_CLEAR = 300;

// Settings Page State
int displayBrightness = 80; // 0-100
int displayVolume = 50;     // 0-100

// Alarm State
int alarmHour = 7;
int alarmMinute = 30;
bool alarmIsPM = false; // false = AM, true = PM
bool alarmEnabled = true;
bool alarmDays[7] = {false, false, false, false, false, false, false}; // Sun, Mon, Tue, Wed, Thu, Fri, Sat
bool alarmSounding = false;
unsigned long alarmStopTime = 0;

// --- OBJECTS ---
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_SERVER, GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC);
DHT dht(DHT_PIN, DHT_TYPE);


// --- FUNCTION PROTOTYPES ---
void sendToNextion(const String &cmd);
void listenToNextion();
void setupWiFi();
void updateTime();
void readSensors();
void updateVisiblePage();
void updateLockScreen();
void updateHomeScreen();
void clearPasswordFieldAndFlush();
void updateAlarmDisplay();
void initializeAlarm();
void checkAlarm();
void triggerAlarm();
void stopAlarm();
void handlePasswordInput(char c);
void handleSettingsInput(char c);
void handleAlarmInput(char c);
inline bool isIgnoring();
String getTextFromNextion_BLOCKING(const String &component);


// --- HELPER FUNCTION IMPLEMENTATIONS ---

/**
 * @brief Sends a command string to the Nextion display, followed by terminators.
 */
void sendToNextion(const String &cmd) {
  nextion.print(cmd);
  nextion.write(0xFF);
  nextion.write(0xFF);
  nextion.write(0xFF);
}

/**
 * @brief Retrieves text from a Nextion component.
 * @warning THIS IS A BLOCKING FUNCTION. It will pause all other code
 * until it gets a reply or times out. Use only when necessary.
 */
String getTextFromNextion_BLOCKING(const String &component) {
  String result = "";
  // Flush any old data in the serial buffer before sending a command
  while (nextion.available()) {
    nextion.read();
  }
  
  // Request the component's text value
  sendToNextion("get " + component + ".txt");

  // Wait for the response from Nextion (this is the blocking part)
  unsigned long startTime = millis();
  while (millis() - startTime < 500) { // 500ms timeout
    if (nextion.available()) {
      // The response for a text get is prefixed with 0x70
      if (nextion.read() == 0x70) {
        while (nextion.available()) {
          char c = nextion.read();
          if (c == 0xFF) { // End of transmission detected
            // The Nextion sends three 0xFF terminators. We've read one,
            // so we'll consume the next two from the buffer.
            if(nextion.available()) nextion.read();
            if(nextion.available()) nextion.read();
            return result;
          }
          result += c;
        }
      }
    }
  }
  Serial.println("Warning: getTextFromNextion_BLOCKING timed out.");
  return ""; // Return empty string on timeout
}


/**
 * @brief Clears password on ESP and Nextion, flushes serial, and sets ignore window.
 */
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

/**
 * @brief Initializes the alarm variables to their default state.
 */
void initializeAlarm() {
    alarmHour = 7;
    alarmMinute = 30;
    alarmIsPM = false; // AM
    alarmEnabled = true; // Alarm system on by default
    for(int i=0; i<7; i++) { alarmDays[i] = false; } // All days off by default
}

/**
 * @brief Updates the visual text on the alarm page (p5_alarm).
 */
void updateAlarmDisplay() {
    char buf[6];
    snprintf(buf, sizeof(buf), "%02d:%02d", alarmHour, alarmMinute);
    sendToNextion("p5_alarm.tAlarm.txt=\"" + String(buf) + "\"");
    sendToNextion("p1_lock.tLockAlarm.txt=\"" + String(buf) + "\""); // Also update lock screen
    
    // Update meridian toggle (btMeridian)
    sendToNextion("p5_alarm.btMeridian.val=" + String(alarmIsPM ? 1 : 0));
    // Update alarm on/off toggle (btState)
    sendToNextion("p5_alarm.btState.val=" + String(alarmEnabled ? 1 : 0));
}

/**
 * @brief Checks if the current time matches the alarm settings.
 */
void checkAlarm() {
    if (!alarmEnabled || alarmSounding) return;
    if (!wifiConnected) return;

    unsigned long epochTime = timeClient.getEpochTime();
    int currentDay = weekday(epochTime) - 1; // TimeLib is 1-7 (Sun-Sat), our array is 0-6
    
    if (!alarmDays[currentDay]) return;
    
    int currentHour = hour(epochTime); // 24-hour format
    int currentMinute = minute(epochTime);
    
    int alarmHour24 = alarmHour;
    if (alarmIsPM && alarmHour24 != 12) {
        alarmHour24 += 12;
    }
    if (!alarmIsPM && alarmHour24 == 12) { // Handle 12 AM
        alarmHour24 = 0;
    }

    if (currentHour == alarmHour24 && currentMinute == alarmMinute) {
        triggerAlarm();
    }
}

void triggerAlarm() {
    Serial.println("ALARM! Triggered.");
    alarmSounding = true;
    sendToNextion("page p6_alarm_active"); // Go to the alarm ringing page
    alarmStopTime = millis() + 30000; // Sound for 30 seconds
}

void stopAlarm() {
    if (!alarmSounding) return;
    Serial.println("Alarm stopped.");
    alarmSounding = false;
    sendToNextion("page p3_home"); // Go back home after stopping
}


/**
 * @brief The main router for commands received from the Nextion display.
 * This function acts as a state machine.
 */
void listenToNextion() {
  if (nextion.available()) {
    char cmd = nextion.read();

    if (isIgnoring() || cmd == 0xFF) {
      return; // Ignore if in debounce window or it's a terminator byte
    }

    // --- STATE CHANGING COMMANDS ---
    // These commands tell the ESP32 which page is now active.
    switch (cmd) {
      case 'p': // Entered Password Page
        Serial.println("STATE -> PASSWORD");
        currentPage = PAGE_PASSWORD;
        clearPasswordFieldAndFlush(); // Clear password when page is entered
        return; 
      case 'P': // Exited Password Page
        Serial.println("STATE -> UNKNOWN");
        currentPage = PAGE_UNKNOWN;
        return;
      case 's': // Entered Settings Page
        Serial.println("STATE -> SETTINGS");
        currentPage = PAGE_SETTINGS;
        return;
      case 'S': // Exited Settings Page
        Serial.println("STATE -> UNKNOWN");
        currentPage = PAGE_UNKNOWN;
        return;
      case 'a': // Entered Alarm Page
        Serial.println("STATE -> ALARM");
        currentPage = PAGE_ALARM;
        updateAlarmDisplay(); // Refresh display with current settings
        return;
      case 'A': // Exited Alarm Page
        Serial.println("STATE -> UNKNOWN");
        currentPage = PAGE_UNKNOWN;
        return;
    }

    // --- STATE-DEPENDENT COMMANDS ---
    // Based on the current page, route the command to the correct handler.
    switch (currentPage) {
      case PAGE_PASSWORD:
        handlePasswordInput(cmd);
        break;
      case PAGE_SETTINGS:
        handleSettingsInput(cmd);
        break;
      case PAGE_ALARM:
        handleAlarmInput(cmd);
        break;
      case PAGE_UNKNOWN:
        // Do nothing if we don't know what page we're on
        break;
    }
  }
}

/**
 * @brief Handles single-character commands from the password page.
 */
void handlePasswordInput(char c) {
  // Digit entry ('0'-'9') is handled by Nextion using "tPassword.txt+=X"
  // so the ESP32 does not need to handle those characters.

  // Backspace ('D') should also be handled on the Nextion side for best performance.
  // In your Backspace button's Touch Press Event, use the command: "bkcmd=1"
  // This tells Nextion to do a backspace on the focused component (tPassword).
  
  if (c == 'O') { // OK button is pressed
    Serial.println("OK received. Requesting password from display...");

    // This function will PAUSE the ESP32 until it gets the text.
    String passFromDisplay = getTextFromNextion_BLOCKING("p2_pass.tPassword");

    Serial.println("Received from display: '" + passFromDisplay + "'");

    if (passFromDisplay == presetPassword) {
      Serial.println("✅ Password correct!");
      sendToNextion("page p3_home");
      clearPasswordFieldAndFlush();
    } else {
      Serial.println("❌ Wrong password!");
      sendToNextion("p2_pass.tPassIncorrect.vis=1"); // Make error visible
      delay(1500); // Show error for 1.5s
      sendToNextion("p2_pass.tPassIncorrect.vis=0"); // Hide it again
      clearPasswordFieldAndFlush();
    }
  }
}

/**
 * @brief Handles single-character commands from the settings page.
 */
void handleSettingsInput(char c) {
    switch(c) {
        case 'b': // Brightness Up
            if (displayBrightness < 100) displayBrightness += 5;
            sendToNextion("dims=" + String(displayBrightness));
            sendToNextion("p4_settings.hBright.val=" + String(displayBrightness));
            break;
        case 'B': // Brightness Down
            if (displayBrightness > 5) displayBrightness -= 5;
            sendToNextion("dims=" + String(displayBrightness));
            sendToNextion("p4_settings.hBright.val=" + String(displayBrightness));
            break;
        case 'v': // Volume Up
            if (displayVolume < 100) displayVolume += 5;
            sendToNextion("vols=" + String(displayVolume));
            sendToNextion("p4_settings.hVol.val=" + String(displayVolume));
            break;
        case 'V': // Volume Down
            if (displayVolume > 0) displayVolume -= 5;
            sendToNextion("vols=" + String(displayVolume));
            sendToNextion("p4_settings.hVol.val=" + String(displayVolume));
            break;
        case 't': // Sound Test
            sendToNextion("play 0,1");
            break;
    }
}

/**
 * @brief Handles single-character commands from the alarm page.
 */
void handleAlarmInput(char c) {
    switch(c) {
        case 'h': alarmHour = (alarmHour % 12) + 1; break; // Hour+
        case 'H': alarmHour = (alarmHour == 1) ? 12 : alarmHour - 1; break; // Hour-
        case 'm': alarmMinute = (alarmMinute + 5) % 60; break; // Min+ (in 5 min steps)
        case 'M': alarmMinute = (alarmMinute < 5) ? (60 + (alarmMinute-5)) : alarmMinute - 5; break; // Min-
        case 't': alarmIsPM = !alarmIsPM; break; // Toggle AM/PM
        case 'e': alarmEnabled = !alarmEnabled; break; // Toggle alarm enable/disable
        case '0': alarmDays[0] = !alarmDays[0]; sendToNextion("p5_alarm.btSun.val="+String(alarmDays[0])); break;
        case '1': alarmDays[1] = !alarmDays[1]; sendToNextion("p5_alarm.btMon.val="+String(alarmDays[1])); break;
        case '2': alarmDays[2] = !alarmDays[2]; sendToNextion("p5_alarm.btTue.val="+String(alarmDays[2])); break;
        case '3': alarmDays[3] = !alarmDays[3]; sendToNextion("p5_alarm.btWed.val="+String(alarmDays[3])); break;
        case '4': alarmDays[4] = !alarmDays[4]; sendToNextion("p5_alarm.btThu.val="+String(alarmDays[4])); break;
        case '5': alarmDays[5] = !alarmDays[5]; sendToNextion("p5_alarm.btFri.val="+String(alarmDays[5])); break;
        case '6': alarmDays[6] = !alarmDays[6]; sendToNextion("p5_alarm.btSat.val="+String(alarmDays[6])); break;
        case 'x': stopAlarm(); break; // Command from the "Stop" button on alarm_active page
    }
    updateAlarmDisplay(); // Update text on screen after any change
}

#endif // CONFIG_H

