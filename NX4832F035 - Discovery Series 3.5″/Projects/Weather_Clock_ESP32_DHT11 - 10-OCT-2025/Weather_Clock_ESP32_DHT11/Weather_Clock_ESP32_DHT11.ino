#include <HardwareSerial.h>
#include <WiFi.h> // Kept for NTP time synchronization
#include "time.h"
#include "DHT.h" // Include the DHT library

// --- DHT Sensor Configuration ---
#define DHTPIN 4      // **CRITICAL: Change this to the physical GPIO pin you are using**
#define DHTTYPE DHT11 // DHT 11 sensor type
DHT dht(DHTPIN, DHTTYPE);

// Nextion display serial port
HardwareSerial Nextion(2); 

// WiFi credentials (kept for time display via NTP)
const char* ssid = "Bhavya";
const char* password = "bhaVYA@02";

// NTP server and time zone settings (kept for time display)
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800; // +5:30 (India Standard Time)
const int daylightOffset_sec = 0; 

// Text components on Nextion display
const char* weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

// Function to send a command to the Nextion display
void sendToNextion(const char* component, const char* value, bool isNumeric = false) {
  Nextion.print(component);
  Nextion.print("=");
  if (!isNumeric) Nextion.print("\"");
  Nextion.print(value);
  if (!isNumeric) Nextion.print("\"");
  Nextion.write(0xFF);
  Nextion.write(0xFF);
  Nextion.write(0xFF);
}

// Function to read DHT11 data and update display
void readDHTData() {
    // Reading sensor data
    float h = dht.readHumidity();
    // DHT11 returns temperature in Celsius by default
    float t = dht.readTemperature(); 

    // Check if any reads failed
    if (isnan(h) || isnan(t)) {
        Serial.println("Failed to read from DHT sensor! Check wiring and library.");
        // Send error placeholders to Nextion
        sendToNextion("tTemp.txt", "--");
        sendToNextion("tHum.txt", "--");
        return;
    }

    // Format strings for display
    char tempStr[6];
    sprintf(tempStr, "%.1f", t); // Temperature to one decimal place
    char humStr[4];
    // DHT11 returns humidity as an integer, so cast and format
    sprintf(humStr, "%d", (int)h); 

    // Send data to Nextion display
    sendToNextion("tTemp.txt", tempStr);
    sendToNextion("tHum.txt", humStr);
    
    Serial.printf("DHT11 Data - Temp: %.1f°C, Hum: %d%%\n", t, (int)h);
}


void setup() {
  Serial.begin(115200);
  // Nextion display communication (using pins 16 and 17 as defined in original code)
  Nextion.begin(9600, SERIAL_8N1, 16, 17);

  // Initialize DHT sensor
  dht.begin();

  delay(3000); 
  
  // --- WiFi Connection for NTP Time Sync ---
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
  
  // --- NTP Time Synchronization ---
  Serial.println("Synchronizing time...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    delay(500);
    Serial.println("Waiting for NTP sync...");
  }
  Serial.println("Time synchronized.");
  delay(2000);

  // Get DHT data on startup
  readDHTData();
}

void loop() {
  // Get current time information
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    // Attempt to re-sync if time is lost
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    return;
  }
  
  // Update time display every second
  char dateString[6];
  strftime(dateString, sizeof(dateString), "%d-%m", &timeinfo);
  char timeString[9];
  strftime(timeString, sizeof(timeString), "%H:%M", &timeinfo);
  int dayOfWeekIndex = timeinfo.tm_wday;
  const char* dayOfWeekString = weekdays[dayOfWeekIndex];

  sendToNextion("tDate.txt", dateString);
  sendToNextion("tDay.txt", dayOfWeekString);
  sendToNextion("tTime.txt", timeString);
  
  // Update sensor data every 5 seconds (DHT11 is slow, so check frequently)
  static unsigned long lastSensorUpdate = 0;
  // Read interval set to 5 seconds (5000 milliseconds)
  if (millis() - lastSensorUpdate > 5000) { 
    readDHTData();
    lastSensorUpdate = millis();
  }

  delay(1000); 
}
