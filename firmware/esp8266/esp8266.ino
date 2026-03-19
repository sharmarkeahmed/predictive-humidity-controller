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
unsigned long apiDelay = 900000;   // 15 minutes
// unsigned long apiDelay = 60000; // 60 Seconds
unsigned long UARTDelay = 10000; // 10 Seconds

// number of forecast hours we want
const int FORECAST_HOURS = 8;

float forecastTemp[FORECAST_HOURS];
uint8_t forecastRH[FORECAST_HOURS];
bool forecastValid = false;

// UART sequence counter
uint8_t sequence = 0;

// State Machine States
enum SystemState {
  INIT,
  WIFI_CONNECT,
  IDLE,
  FETCH_API,
  PROCESS_DATA,
  ERROR
};

// Initialize the current state as initializing
SystemState currentState = INIT;

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
  
  // Case state for the ESP to avoid blocking loop statements, this allows for setting different delays for the API and UART
  switch(currentState) {

    case INIT:
      Serial.println("Initializing...");
      currentState = WIFI_CONNECT;
      break;

    case WIFI_CONNECT:
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi Connected");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
        currentState = IDLE;
      } else {
        Serial.print("Could not connect to WIFI");
        currentState = INIT;
      }

      break;

    case IDLE:
      if ((currentTime - lastApiTime) > apiDelay) {
        currentState = FETCH_API;
      }
      break;

    case FETCH_API: {

      String serverPath =
        "https://api.open-meteo.com/v1/forecast?latitude=" + latitude +
        "&longitude=" + longitude +
        "&hourly=temperature_2m,relative_humidity_2m" +
        "&current=temperature_2m,relative_humidity_2m" +
        "&timezone=America%2FChicago" +
        "&forecast_days=3" +
        "&temperature_unit=" + temperature_unit;

      jsonBuffer = getHTTPRequest(serverPath.c_str());

      currentState = PROCESS_DATA;
      break;
    }

    case PROCESS_DATA:
      if (parseForecast(jsonBuffer)) {
        forecastValid = true;
        Serial.println("Forecast updated");
        currentState = IDLE;
      } else {
        Serial.println("Parse failed");
        currentState = ERROR;
      }
      lastApiTime = currentTime;
      break;

    case ERROR:
      Serial.println("Error occurred, retrying...");
      currentState = IDLE;
      break;
  }
 
  // UART Task
  if ((currentTime - lastUartTime) > UARTDelay && forecastValid) {
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
  // Parse the JSON Data and store it into a JSON variable
  JSONVar weatherForecast = JSON.parse(json);
  if (JSON.typeof(weatherForecast) == "undefined") return false;

  // Grab the current time
  JSONVar current = weatherForecast["current"];
  String currentTime = (const char*) current["time"];

  // Grab the hourly Times, temperatures
  JSONVar hourly = weatherForecast["hourly"];
  JSONVar times = hourly["time"];
  JSONVar temperature = hourly["temperature_2m"];
  JSONVar humidity = hourly["relative_humidity_2m"];

  // Find the start index which should be sent to the STM32, This is the hour of your current time
  int startIndex = 0;
  for (int i = 0; i < times.length(); i++) {
    if (String((const char*)times[i]) == currentTime) {
      startIndex = i;
      break;
    }
  }

// Store the forecasts into forecastTemp and forecastRH for the number of FORECAST_HOURS
  for (int i = 0; i < FORECAST_HOURS; i++) {
    forecastTemp[i] = (double) temperature[startIndex + i];
    forecastRH[i] = (int) humidity[startIndex + i];

    Serial.print("Hour ");
    Serial.print(i);
    Serial.print(" | Temp: ");
    Serial.print(forecastTemp[i]);
    Serial.print(" F | RH: ");
    Serial.println(forecastRH[i]);
  }

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
  sequence++;   // For Debugging to see what packet we are on (number wise)
  uint8_t buffer[128];
  int index = 0;
  int lengthIndex = index++;      // reserve length

  // Payload [START 0XAA][LEN (in BYTES)][DATA][CHECKSUM][END 0X55]

  // Store temperatures as floats
  for (int i = 0; i < FORECAST_HOURS; i++) {
    memcpy(&buffer[index], &forecastTemp[i], sizeof(float));
    index += sizeof(float);
  }

  // Store Relative humidity as integers
  for (int i = 0; i < FORECAST_HOURS; i++) {
    buffer[index++] = forecastRH[i];
  }

  // Store length of data
  uint8_t payloadLength = index - 1;
  buffer[lengthIndex] = payloadLength;

  uint8_t checksum = computeChecksum(buffer, index);
  buffer[index++] = checksum;

  // Send the frame
  Serial1.write(0xAA);
  Serial1.write(buffer, index);
  Serial1.write(0x55);

  Serial.print("UART sent | Seq: ");
  Serial.println(sequence);
}

/* Old function
void sendForecastUART(){
  Serial.println("Sending forecast to STM32...");
  Serial1.write(0xAA);
  // Start Byte - use an if else on STM side to check
  // send temperatures (floats = 4 bytes each)
  for(int i = 0; i < FORECAST_HOURS; i++){
    Serial1.write((uint8_t*)&forecastTemp[i], sizeof(float));
  } // send humidity (1 byte each) 
  for(int i = 0; i < FORECAST_HOURS; i++){ 
    Serial1.write(forecastRH[i]);
    }
    Serial1.write(0x55); // END BYTE }
}
*/
