#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Arduino_JSON.h>

#define STM_BAUD 115200

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
unsigned long lastApiTime = 0;
unsigned long lastUartTime = 0;
// unsigned long apiDelay = 900000;   // 15 minutes
unsigned long apiDelay = 60000; // 60 Seconds
unsigned long UARTDelay = 10000; // 10 Seconds

// number of forecast hours we want
const int FORECAST_HOURS = 8;

float forecastTemp[FORECAST_HOURS];
uint8_t forecastRH[FORECAST_HOURS];
bool forecastValid = false;
uint8_t forecastStartHour = 0;

// UART sequence counter
uint8_t sequence = 0;


String getHTTPRequest(const char* serverName); // Get HTTP request from API serverName = resource URL
bool parseForecast(String json); // Takes in the raw forecast returned from the API and turns it into forecast temperature and humidity arrays
void sendForecastUART(); // Sends Forecast data over UART
uint8_t computeChecksum(uint8_t* data, int len);

/*
ESP8266 UART:
Serial  -> USB debug
Serial1 -> TX only (GPIO2 / D4) → STM32 RX (PA10)
*/

void setup() {

  Serial.begin(115200); // Begins serial communication with Serial Monitor
  Serial1.begin(STM_BAUD);

  Serial.println("UART Initialized");

  delay(1000);

  // Start WiFi communication and connect
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  Serial.println("System starting...");
}

void loop() {

  unsigned long currentTime = millis();

  // API Task
  if ((currentTime - lastApiTime) > apiDelay) {

    if (WiFi.status() == WL_CONNECTED) {

      String serverPath =
      "https://api.open-meteo.com/v1/forecast?latitude=" + latitude +
      "&longitude=" + longitude +
      "&hourly=temperature_2m,relative_humidity_2m" +
      "&current=temperature_2m,relative_humidity_2m" +
      "&timezone=America%2FChicago" +
      "&forecast_days=3" +
      "&temperature_unit=" + temperature_unit;

      jsonBuffer = getHTTPRequest(serverPath.c_str());

      Serial.println("JSON received");

      parseForecast(jsonBuffer);

    } else {

      Serial.println("WiFi lost, reconnecting...");
      WiFi.begin(WIFI_SSID, WIFI_PASS);

    }

    lastApiTime = currentTime;
  }

  // UART Task
  if ((currentTime - lastUartTime) > UARTDelay) {
    sendForecastUART();
    lastUartTime = currentTime;
  }
}

// HTTP request function, gets resources from servers at specified server names/URLs and returns them as a JSON string
// Helper function for main in the FETCH_API state
String getHTTPRequest(const char* serverName){

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.begin(client, serverName);

  int httpResponseCode = http.GET();

  String payload = "{}";

  if (httpResponseCode > 0) {
    payload = http.getString();
  } else {
    Serial.print("HTTP error: ");
    Serial.println(httpResponseCode);
  }

  http.end();
  return payload;
}

// Helper function to parse the weather forecast data returned by the API server
// Saves the weather data into arrays 
bool parseForecast(String json) {
    JSONVar weatherForecast = JSON.parse(json);
    if (JSON.typeof(weatherForecast) == "undefined") return false;

    JSONVar current = weatherForecast["current"];
    String currentTime = (const char*) current["time"];  // "2026-03-26T12:30"

    // Truncate to hour boundary: "2026-03-26T12:30" → "2026-03-26T12:00"
    // The hour string is always the first 13 chars + ":00"
    String currentHour = currentTime.substring(0, 13) + ":00";

    JSONVar hourly = weatherForecast["hourly"];
    JSONVar times = hourly["time"];
    JSONVar temperature = hourly["temperature_2m"];
    JSONVar humidity = hourly["relative_humidity_2m"];

    int startIndex = 0;
    
    bool found = false;
    for (int i = 0; i < (int)times.length(); i++) {
        if (String((const char*)times[i]) == currentHour) {
            startIndex = i;
            found = true;
            break;
        }
    }

    if (!found) {
        Serial.println("Warning: current hour not found in forecast, defaulting to index 0");
    }

    // Extract the hour from the matched time string "2026-03-26T12:00"
    // Character index:                                0123456789012345
    // The hour is always at index 11-12
    String hourStr = currentHour.substring(11, 13);
    forecastStartHour = (uint8_t)hourStr.toInt();

    Serial.print("Forecast start hour: ");
    Serial.println(forecastStartHour);

    Serial.print("Start index: ");
    Serial.print(startIndex);
    Serial.print(" | Current hour: ");
    Serial.println(currentHour);

    for (int i = 0; i < FORECAST_HOURS; i++) {
        forecastTemp[i] = (double) temperature[startIndex + i];
        forecastRH[i]   = (int)    humidity[startIndex + i];

        Serial.print("Hour +"); Serial.print(i);
        Serial.print(" | Temp: "); Serial.print(forecastTemp[i]);
        Serial.print(" F | RH: "); Serial.println(forecastRH[i]);
    }

    forecastValid = true;
    return true;
}

// Checksum - catches corruptions/bits flipped in the data, Can use this in the STM32 side to see if this cs is the same as the STM cs
// Must recompute sum of all data on STM side, this will help prevent bit flips/data corruption
uint8_t computeChecksum(uint8_t* data, int len) {
  uint8_t cs = 0;
  for (int i = 0; i < len; i++) {
    cs ^= data[i];
  }
  return cs;
}

// Send UART data to the STM32
void sendForecastUART() {

  Serial.println("Sending forecast to STM32...");

  const int PACKET_SIZE =
      1 + 1 + (FORECAST_HOURS * sizeof(float)) + FORECAST_HOURS + 1;

  uint8_t packet[PACKET_SIZE];
  int index = 0;

  packet[index++] = 0xAA;
  packet[index++] = forecastStartHour;     // Hour (0-23)


  for(int i = 0; i < FORECAST_HOURS; i++){
    memcpy(&packet[index], &forecastTemp[i], sizeof(float));
    index += sizeof(float);
  }

  for(int i = 0; i < FORECAST_HOURS; i++){
    packet[index++] = forecastRH[i];
  }

  packet[index++] = 0x55;

  Serial1.write(packet, PACKET_SIZE);

  Serial.println("Packet:");
  for(int i = 0; i < PACKET_SIZE; i++){
    if(packet[i] < 16) Serial.print("0");
    Serial.print(packet[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}

