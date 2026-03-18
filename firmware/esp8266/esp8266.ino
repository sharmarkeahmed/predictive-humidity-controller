#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Arduino_JSON.h>

#define STM_BAUD 9600

// const char* WIFI_SSID = "LIT-FAM";
// const char* WIFI_PASS = "Demell2023";

const char* WIFI_SSID = "UofM-IoT";
const char* WIFI_PASS = "marasmus6operator";

// For the weather API endpoint
String latitude = "44.9716";
String longitude = "-93.2344";
String temperature_unit = "fahrenheit";
String jsonBuffer;

// Delay for calling the API
unsigned long lastTime = 0;
// unsigned long timerDelay = 900000;   // 15 minutes
unsigned long timerDelay = 6000;

// number of forecast hours we want
const int FORECAST_HOURS = 8;

float forecastTemp[FORECAST_HOURS];
uint8_t forecastRH[FORECAST_HOURS];

/*
ESP8266 UART:
Serial  -> USB debug
Serial1 -> TX only (GPIO2 / D4) → STM32 RX (PA10)
*/

// Function to send forecast over UART
void sendForecastUART(){

  Serial.println("Sending forecast to STM32...");

  Serial1.write(0xAA);   // Start Byte - use an if else on STM side to check

  // send temperatures (floats = 4 bytes each)
  for(int i = 0; i < FORECAST_HOURS; i++){
    Serial1.write((uint8_t*)&forecastTemp[i], sizeof(float));
  }

  // send humidity (1 byte each)
  for(int i = 0; i < FORECAST_HOURS; i++){
    Serial1.write(forecastRH[i]);
  }

  Serial1.write(0x55);   // END BYTE
}

// HTTP request function, gets resources from servers at specified server names/URLs
String getHTTPRequest(const char* serverName){

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.begin(client, serverName);

  int httpResponseCode = http.GET();

  String payload = "{}";

  if (httpResponseCode > 0) {
    payload = http.getString();
  }

  http.end();
  return payload;
}

void setup() {

  Serial.begin(115200);
  Serial1.begin(STM_BAUD);   // UART to STM32 (GPIO2)

  delay(1000);

  // Start WiFi communication and connect
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  Serial.print("Connecting");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  // If statement helps call the API only for the specified timerDelay to prevent overcalling/reaching API limits on open meteo
  if ((millis() - lastTime) > timerDelay) {

    if (WiFi.status() == WL_CONNECTED) {

      String serverPath =
      "https://api.open-meteo.com/v1/forecast?latitude=" + latitude +
      "&longitude=" + longitude +
      "&hourly=temperature_2m,relative_humidity_2m" +
      "&current=temperature_2m,relative_humidity_2m" +
      "&timezone=America%2FChicago" +
      "&forecast_days=3" +
      "&temperature_unit=" + temperature_unit;
      
      // Store the JSON data into a JSON buffer to parse the temperature and humidity data into arrays
      jsonBuffer = getHTTPRequest(serverPath.c_str());

      Serial.println("JSON received");

      JSONVar weatherForecast = JSON.parse(jsonBuffer);

      if (JSON.typeof(weatherForecast) == "undefined") {
        Serial.println("JSON parsing failed");
        return;
      }

      JSONVar current = weatherForecast["current"];
      String currentTime = (const char*) current["time"];

      JSONVar hourly = weatherForecast["hourly"];
      JSONVar times = hourly["time"];
      JSONVar temperature = hourly["temperature_2m"];
      JSONVar humidity = hourly["relative_humidity_2m"];

      int startIndex = 0;
      int totalHours = times.length();

      for (int i = 0; i < totalHours; i++) {

        String t = (const char*) times[i];

        if (t == currentTime) {
          startIndex = i;
          break;
        }
      }

      // Following print statements are for debugging the api data parsing, and printing them to the arduino serial monitor. 
      Serial.print("Current index = ");
      Serial.println(startIndex);

      for (int i = 0; i < FORECAST_HOURS; i++) {

        forecastTemp[i] = (double) temperature[startIndex + i];
        forecastRH[i] = (int) humidity[startIndex + i];

        Serial.print("Hour ");
        Serial.print(i);
        Serial.print(" | Temp = ");
        Serial.print(forecastTemp[i]);
        Serial.print(" F | RH = ");
        Serial.println(forecastRH[i]);
      }

      // SEND TO STM32
      sendForecastUART();
    }

    else {
      Serial.println("WiFi lost, reconnecting...");
      WiFi.begin(WIFI_SSID, WIFI_PASS);
    }

    lastTime = millis();
  }
}
