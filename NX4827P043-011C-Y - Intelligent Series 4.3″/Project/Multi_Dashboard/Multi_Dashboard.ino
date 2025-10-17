/**
 * @file NextionDashboard.ino
 * @author Gemini
 * @brief Main logic file for the 4.3" Nextion Intelligent Series Dashboard.
 * * This file contains the primary setup() and loop() functions, along with the 
 * core logic for handling time updates, sensor readings, and Nextion communication.
 * It works in tandem with `config.h`, which defines all variables, pins, and helper functions.
 * * Board: ESP32 Dev Module (or similar)
 */

#include "config.h"

// --- SETUP ---
void setup() {
  // Start serial for debugging
  Serial.begin(115200);
  Serial.println("\n--- Nextion Dashboard Initializing ---");

  // Start serial for Nextion display
  // Using pins defined in config.h (GPIO 16 for RX, 17 for TX)
  nextion.begin(9600, SERIAL_8N1, NEXTION_RX_PIN, NEXTION_TX_PIN);

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
  } else {
    Serial.println("WiFi not connected. Time will not be synced.");
  }
  
  // Set initial brightness and volume on the display
  sendToNextion("dims=" + String(displayBrightness));
  sendToNextion("vols=" + String(displayVolume));

  Serial.println("--- Initialization Complete. Running. ---");
}

// --- MAIN LOOP ---
void loop() {
  // 1. Listen for and handle any incoming commands from the Nextion display
  listenToNextion();

  // 2. Non-blocking check to update time every second
  if (millis() - lastTimeUpdate >= 1000) {
    updateTime();
    lastTimeUpdate = millis();
  }

  // 3. Non-blocking check to read sensor data every 5 seconds
  if (millis() - lastSensorUpdate >= 5000) {
    readSensors();
    lastSensorUpdate = millis();
  }

  // 4. Non-blocking check to update the widgets on the current page
  // This ensures we only send data for the visible page, saving bandwidth.
  if (millis() - lastDisplayUpdate >= 1000) {
    updateVisiblePage();
    lastDisplayUpdate = millis();
  }
  
  // 5. Check if the alarm needs to be triggered
  checkAlarm();
}

// --- CORE FUNCTIONS IMPLEMENTATION ---

/**
 * @brief Sets up the WiFi connection and updates the lock screen status.
 */
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

/**
 * @brief Fetches and formats the current time from the NTP client.
 */
void updateTime() {
  if (!wifiConnected) return; // Can't get time without WiFi

  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();
  
  // Format time for lock screen (e.g., 10:30, PM, Tue, Oct 14)
  snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d", hour(epochTime), minute(epochTime));
  lockTime = timeBuffer;
  lockMeridian = (isAM(epochTime)) ? "AM" : "PM";
  snprintf(timeBuffer, sizeof(timeBuffer), "%.3s, %.3s %d", dayShortStr(weekday(epochTime)), monthShortStr(month(epochTime)), day(epochTime));
  lockDayDate = timeBuffer;

  // Format time for home screen (e.g., 10, 30, PM, 14-10, Tue)
  snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d", hourFormat12(epochTime), minute(epochTime));
  homeTime = timeBuffer;
  homeMeridian = lockMeridian;
  snprintf(timeBuffer, sizeof(timeBuffer), "%02d-%02d", day(epochTime), month(epochTime));
  homeDate = timeBuffer;
  snprintf(timeBuffer, sizeof(timeBuffer), "%.3s", dayShortStr(weekday(epochTime)));
  homeDay = timeBuffer;
}

/**
 * @brief Reads temperature and humidity from the DHT11 sensor.
 */
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

/**
 * @brief Main function to update all widgets on the currently visible page.
 */
void updateVisiblePage() {
    // We can update all key widgets. The Nextion display is smart enough
    // to only process commands for the currently loaded page.
    updateLockScreen();
    updateHomeScreen();
    // The alarm page is updated instantly when buttons are pressed, so no need for a timed update.
}

/**
 * @brief Sends commands to update all visual elements on the Lock Screen.
 */
void updateLockScreen() {
    sendToNextion("p1_lock.tLockTime.txt=\"" + lockTime + "\"");
    sendToNextion("p1_lock.tLockMeridian.txt=\"" + lockMeridian + "\"");
    sendToNextion("p1_lock.tLockDayDate.txt=\"" + lockDayDate + "\"");
    sendToNextion("p1_lock.tLockTemp.txt=\"" + String(temperature) + "°C\"");
    // Alarm time is static unless changed on alarm page
}

/**
 * @brief Sends commands to update all visual elements on the Home Screen.
 */
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
