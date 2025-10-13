#include <HardwareSerial.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "time.h"

// Nextion display serial port
HardwareSerial Nextion(2); 

// WiFi credentials
const char* ssid = "Bhavya";
const char* password = "bhaVYA@02";

// WeatherAPI.com credentials
const char* weatherApiKey = "4e45c96410024d24b0783152251210";
const char* location = "Kamrej";

// NTP server and time zone settings
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800; 
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

// Function to get weather data and update display
void getWeatherData() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String serverPath = "http://api.weatherapi.com/v1/current.json?key=" + String(weatherApiKey) + "&q=" + String(location);
    http.begin(serverPath.c_str());
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);

      // Extract required values for display
      float temperature = doc["current"]["temp_c"];
      int humidity = doc["current"]["humidity"];
      float visibility_km = doc["current"]["vis_km"];
      int visibility_m = static_cast<int>(visibility_km * 1000);
      
      // Get the weather condition code
      int conditionCode = doc["current"]["condition"]["code"];

      // Format strings for display
      char tempStr[6];
      sprintf(tempStr, "%.1f", temperature);
      char humStr[4];
      sprintf(humStr, "%d", humidity);
      char visStr[7];
      sprintf(visStr, "%d", visibility_m);

      // Send data to Nextion display
      sendToNextion("tTemp.txt", tempStr);
      sendToNextion("tHum.txt", humStr);
      sendToNextion("tVis.txt", visStr);
      
      int picNumber = 2; // Default to '2 clouds with wind'

      if (conditionCode == 1063 || conditionCode == 1180 || conditionCode == 1183 || conditionCode == 1186 || conditionCode == 1189 || conditionCode == 1192 || conditionCode == 1195) {
        // Light rain
        picNumber = 3; // 'clouds with rain'
      } else if (conditionCode == 1087 || conditionCode == 1273 || conditionCode == 1276 || conditionCode == 1279 || conditionCode == 1282) {
        // Thunderstorm or heavy rain
        picNumber = 4; // 'cloud, rain and thunder'
      }

      char picStr[2];
      sprintf(picStr, "%d", picNumber);
      sendToNextion("p1.pic", picStr, true);
      
      Serial.println("Weather data and picture updated.");
    } else {
      Serial.println("HTTP request failed.");
    }
    http.end();
  }
}

void setup() {
  Serial.begin(115200);
  Nextion.begin(9600, SERIAL_8N1, 16, 17);

  delay(3000); 

  sendToNextion("p1.pic", "5", true);
  
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
  Serial.println("Synchronizing time...");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    delay(500);
    Serial.println("Waiting for NTP sync...");
  }
  Serial.println("Time synchronized.");
  delay(2000);
  sendToNextion("p1.pic", "1", true);
  delay(2000);
  // Get weather data on startup
  getWeatherData();
}

void loop() {
  // Get current time information
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
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
  
  // Update weather every 15 minutes (900000 milliseconds)
  static unsigned long lastWeatherUpdate = 0;
  if (millis() - lastWeatherUpdate > 900000) {
    getWeatherData();
    lastWeatherUpdate = millis();
  }

  delay(1000); 
}