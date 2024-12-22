#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <DHT.h>
#include <LittleFS.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Arduino_JSON.h>
#include <ElegantOTA.h>

#define DHTTYPE DHT11

bool builtinLEDState = false;

const int redLEDPin = D6;
const int yellowLEDPin = D7;
const int greenLEDPin = D8;

const int dht11SensorPin = D1;
const int soilMoistureSensorPin = D2;
const int waterLevelSensorPin = A0;
const int waterPumpSensorPin = D5;

int dhtSensorTemperatureValue = 0;
int dhtSensorHumidityValue = 0;
int soilMoistureSensorValue = 0;
int waterLevelSensorValue = 0;

bool onWaterPump = false;

const int waterLevelThreshold = 300;

// WiFi Credentials
const char *ssid = "Nighthawks0230";
const char *password = "liew1212";
const char *hostname = "ESP8266";

// Set static IP address
IPAddress local_ip(192, 168, 0, 123);
// Set gateway IP address
IPAddress gateway(192, 168, 0, 1);
// Set network subnet mask
IPAddress subnetMask(255, 255, 255, 0);
// Set DNS server IP address
IPAddress primaryDNS(8, 8, 8, 8);

AsyncWebServer server(80);

AsyncWebSocket ws("/ws");

bool notifying = false;

JSONVar readings;

DHT dht(dht11SensorPin, DHTTYPE);

unsigned long now = millis();
unsigned long timerDelay = 2000;
unsigned long ota_progress_millis = 0;

void onOTAStart()
{
  // Log when OTA has started
  Serial.println("OTA update started!");
}

void onOTAProgress(size_t current, size_t final) {
  // Log every 1 second
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}

void onOTAEnd(bool success)
{
  // Log when OTA has finished
  if (success)
  {
    Serial.println("OTA update finished successfully!");
  }
  else
  {
    Serial.println("There was an error during OTA update!");
  }
  // <Add your own code here>
}

String getSensorReadings()
{
  dhtSensorTemperatureValue = dht.readTemperature();
  dhtSensorHumidityValue = dht.readHumidity();
  // soilMoistureSensorValue = digitalRead(soilMoistureSensorPin);
  soilMoistureSensorValue = 0;
  waterLevelSensorValue = analogRead(waterLevelSensorPin);

  readings["temperature"] = String(dhtSensorTemperatureValue);
  readings["humidity"] = String(dhtSensorHumidityValue);
  readings["water_level"] = String(map(waterLevelSensorValue, 0, 1024, 0, 100));
  readings["soil_moisture"] = String(soilMoistureSensorValue);
  readings["water_pump"] = String(onWaterPump);
  String jsonString = JSON.stringify(readings);

  return jsonString;
}

void initFS()
{
  if (!LittleFS.begin())
  {
    Serial.println("An error has occured while mounting LittleFS");
  }
  else
  {
    Serial.println("LittleFS mounterd successfully");
  }
}

void setup_wifi()
{
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.hostname(hostname);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Hostname: ");
  Serial.println(WiFi.getHostname());
}

// void notifyClients(String sensorReadings)
// {
//   ws.textAll(sensorReadings);
// }

void notifyClients(String sensorReadings)
{
  if (notifying)
  {
    return; // If already notifying, skip this call
  }

  notifying = true;
  ws.textAll(sensorReadings);
  notifying = false;
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  {
    data[len] = 0;
    if (strcmp((char *)data, "getReadings") == 0)
    {
      // if it is, send current sensor readings
      Serial.println("Get sensor readings from the handleWebSocketMessage");
      String sensorReadings = getSensorReadings();
      Serial.print(sensorReadings);
      notifyClients(sensorReadings);
    }
    else if (strcmp((char *)data, "toggle") == 0)
    {
      Serial.println("Toggle Water Pump");
      onWaterPump = !onWaterPump;
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  switch (type)
  {
  case WS_EVT_CONNECT:
    Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    break;
  case WS_EVT_DISCONNECT:
    Serial.printf("WebSocket client #%u disconnected\n", client->id());
    break;
  case WS_EVT_DATA:
    handleWebSocketMessage(arg, data, len);
    break;
  case WS_EVT_PING:
  case WS_EVT_PONG:
  case WS_EVT_ERROR:
    break;
  }
}

void initWebSocket()
{
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Start board by connect wifi");

  if (!WiFi.config(local_ip, gateway, subnetMask, primaryDNS))
  {
    Serial.println("STA Failed to configure");
  }
  setup_wifi();
  initFS();
  initWebSocket();

  // Setup Red LED Pin mode
  pinMode(redLEDPin, OUTPUT);
  digitalWrite(redLEDPin, LOW);

  // Setup Yellow LED Pin mode
  pinMode(yellowLEDPin, OUTPUT);
  digitalWrite(yellowLEDPin, LOW);

  // Setup Green LED Pin mode
  pinMode(greenLEDPin, OUTPUT);
  digitalWrite(greenLEDPin, LOW);

  // Setup the Water Pump pin mode
  pinMode(waterPumpSensorPin, OUTPUT);
  digitalWrite(waterPumpSensorPin, LOW);

  // pinMode(LED_BUILTIN, OUTPUT);
  // digitalWrite(LED_BUILTIN, LOW);

  // Setup Soli Moisture Sensor Pin Mode
  pinMode(soilMoistureSensorPin, INPUT);

  // Setup Water Level Sensor Pin Mode
  pinMode(waterLevelSensorPin, INPUT);

  // Start the DHT11 Sensor
  dht.begin();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/index.html", "text/html"); });

  server.serveStatic("/", LittleFS, "/");

  ElegantOTA.begin(&server);
  // ElegantOAT callbacks
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);

  server.begin();
}

void loop()
{
  ElegantOTA.loop();
  if ((millis() - now) > timerDelay)
  {
    String sensorReadings = getSensorReadings();
    Serial.println(sensorReadings);
    Serial.println(waterLevelSensorValue);
    notifyClients(sensorReadings);
    now = millis();
    // builtinLEDState = !builtinLEDState;
    // digitalWrite(LED_BUILTIN, builtinLEDState);
  }

  // 1. Monitoring soil moisture levels.
  // If soil moisture is higher mean the soil is dry
  // Open the water pump and turn on the yellow LED pin if soil moisture value is higher than 2048
  if (soilMoistureSensorValue == 1)
  {
    Serial.println("Turn on the yellow LED because the soil is dry");
    onWaterPump = true;
    digitalWrite(yellowLEDPin, HIGH);
  }
  else
  {
    // Serial.println("Turn off the yellow LED because the soil is wet");
    // onWaterPump = false;
    digitalWrite(yellowLEDPin, LOW);
  }

  // Tracking water reservoir levels
  // If more water the sensor is sank in, the higher the output voltage of the pin
  // Stop the water pump and turn on the red LED pin if the water level sensor value is drop under waterLevelThreshold
  if (waterLevelSensorValue < waterLevelThreshold)
  {
    // Serial.println("Turn on the red LED because the water level is low");

    // onWaterPump = false;
    // digitalWrite(waterPumpSensorPin, LOW);
    digitalWrite(redLEDPin, HIGH);
  }
  else
  {
    // Serial.println("Turn off the red LED because the water level is high");
    digitalWrite(redLEDPin, LOW);
  }

  // Automated watering functionality.
  // Turn on the water pump and greed LED pin if the soil is dry
  if (onWaterPump)
  {
    // Serial.println("Turn on the green LED because the water pump is on");
    digitalWrite(waterPumpSensorPin, HIGH);
    digitalWrite(greenLEDPin, HIGH);
  }
  else
  {
    // Serial.println("Turn off the green LED because the water pump is off");
    digitalWrite(waterPumpSensorPin, LOW);
    digitalWrite(greenLEDPin, LOW);
  }

  ws.cleanupClients();

  // now = millis();

  // dhtSensorTemperatureValue = dht.readTemperature();
  // dhtSensorTemperatureValue = dht.readHumidity();
  // // soilMoistureSensorValue = analogRead(soilMoistureSensorPin);
  // soilMoistureSensorValue = digitalRead(soilMoistureSensorPin);
  // waterLevelSensorValue = analogRead(waterLevelSensorPin);

  // Serial.print("Humidity: ");
  // Serial.print(dhtSensorHumidityValue);
  // Serial.println(" %");

  // Serial.print("Temperature: ");
  // Serial.print(dhtSensorTemperatureValue);
  // Serial.println(" °C");

  // Serial.print("Soil Moisture: ");
  // Serial.println(soilMoistureSensorValue);

  // Serial.print("Water Level: ");
  // Serial.println(waterLevelSensorValue);

  // // 1. Monitoring soil moisture levels.
  // // If soil moisture is higher mean the soil is dry
  // // Open the water pump and turn on the yellow LED pin if soil moisture value is higher than 2048
  // if (soilMoistureSensorValue == 1)
  // {
  //   Serial.println("Turn on the yellow LED because the soil is dry");
  //   digitalWrite(yellowLEDPin, HIGH);
  //   onWaterPump = true;
  // }
  // else
  // {
  //   Serial.println("Turn off the yellow LED because the soil is wet");
  //   digitalWrite(yellowLEDPin, LOW);
  //   onWaterPump = false;
  // }

  // // Tracking water reservoir levels
  // // If more water the sensor is sank in, the higher the output voltage of the pin
  // // Stop the water pump and turn on the red LED pin if the water level sensor value is drop under waterLevelThreshold
  // if (waterLevelSensorPin < waterLevelThreshold)
  // {
  //   Serial.println("Turn on the red LED because the water level is low");
  //   digitalWrite(waterPumpSensorPin, LOW);
  //   digitalWrite(redLEDPin, HIGH);
  // }
  // else
  // {
  //   Serial.println("Turn off the red LED because the water level is high");
  //   digitalWrite(redLEDPin, LOW);
  // }

  // // Automated watering functionality.
  // // Turn on the water pump and greed LED pin if the soil is dry
  // if (onWaterPump)
  // {
  //   Serial.println("Turn on the green LED because the water pump is on");
  //   digitalWrite(waterPumpSensorPin, HIGH);
  //   digitalWrite(greenLEDPin, HIGH);
  // }
  // else
  // {
  //   Serial.println("Turn off the green LED because the water pump is off");
  //   digitalWrite(waterPumpSensorPin, LOW);
  //   digitalWrite(greenLEDPin, LOW);
  // }
  // delay(2000);
}

// Web Server
// #include <Arduino.h>
// #include <ESP8266WiFi.h>
// #include <ESPAsyncWebServer.h>
// #include <ESPAsyncTCP.h>
// #include <FS.h>
// #

// const char *ssid = "Nighthawks0230";
// const char *password = "liew1212";

// AsyncWebServer server(80);

// String processor(const String &var)
// {
//   return String();
// }

// void setup()
// {
//   Serial.begin(115200);

//   if (!SPIFFS.begin())
//   {
//     Serial.println("An error has occured while mounting SPIFFS");
//     return;
//   }

//   WiFi.begin(ssid, password);
//   while (WiFi.status() != WL_CONNECTED)
//   {
//     delay(1000);
//     Serial.println("Connecting to WiFi...");
//   }

//   Serial.println(WiFi.localIP());

//   server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
//             { request->send(SPIFFS, "/index.html", String(), false, processor); });

//   server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request)
//             { request->send(SPIFFS, "/style.css", "text/css"); });

//   server.begin();
// }

// void loop()
// {
// }

// Elegant OTA
// #include <Arduino.h>
// #include <ESP8266WiFi.h>
// #include <ESPAsyncTCP.h>
// // #include <ESPAsyncWebServer.h>
// #include <ESP8266WebServer.h>
// #include <ElegantOTA.h>

// const char *ssid = "Nighthawks0230";
// const char *password = "liew1212";

// ESP8266WebServer server(80);

// void setup(void)
// {
//   Serial.begin(115200);
//   WiFi.mode(WIFI_STA);
//   WiFi.begin(ssid, password);
//   Serial.println("");

//   // Wait for connection
//   while (WiFi.status() != WL_CONNECTED)
//   {
//     delay(500);
//     Serial.print(".");
//   }
//   Serial.println("");
//   Serial.print("Connected to ");
//   Serial.println(ssid);
//   Serial.print("IP address: ");
//   Serial.println(WiFi.localIP());

//   server.on("/", []()
//             { server.send(200, "text/plain", "Hi! This is ElegantOTA Demo."); });

//   ElegantOTA.begin(&server); // Start ElegantOTA
//   server.begin();
//   Serial.println("HTTP server started");
// }

// void loop()
// {
//   server.handleClient();
//   ElegantOTA.loop();
// }

// OTA Website
// #include <ESP8266WiFi.h>
// #include <ESP8266mDNS.h>
// #include <WiFiUdp.h>
// #include <Updater.h>
// #include <ESPAsyncWebServer.h>
// #include <ESPAsyncTCP.h>
// #include <FS.h>

// const char *host = "nighthawks";
// const char *ssid = "Nighthawks0230";
// const char *password = "liew1212";

// AsyncWebServer server(80);

// String processor(const String &var)
// {
//   return String();
// }

// void setup()
// {
//   Serial.begin(115200);

//   // pinMode(2, OUTPUT);

//   if (!SPIFFS.begin())
//   {
//     Serial.println("An error has occured while mounting SPIFFS");
//     return;
//   }

//   WiFi.begin(ssid, password);
//   while (WiFi.status() != WL_CONNECTED)
//   {
//     delay(1000);
//     Serial.println("Connecting to WiFi...");
//   }

//   Serial.println(WiFi.localIP());

//   if (!MDNS.begin(host))
//   {
//     Serial.println("Error setting up MDNS responder");
//     while (1)
//     {
//       delay(1000);
//     }
//   }

//   Serial.println("mDNS responder started");

//   server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
//             { request->send(SPIFFS, "/index.html", String(), false, processor); });

//   server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request)
//             { request->send(SPIFFS, "/style.css", "text/css"); });

//   // // Handle firmware update
//   // server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request)
//   //           {
//   //   if (Update.hasError()) {
//   //     request->send(200, "text/plain", "FAIL");
//   //   } else {
//   //     request->send(200, "text/plain", "OK");
//   //     ESP.restart();
//   //   } }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
//   //           {
//   //   if (!index) {
//   //     Serial.printf("Update: %s\n", filename.c_str());
//   //     uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
//   //     if (!Update.begin(maxSketchSpace,U_FS)) { // Start with max available size
//   //       Update.printError(Serial);
//   //     }
//   //   }
//   //   if (Update.write(data, len) != len) {
//   //     Update.printError(Serial);
//   //   }
//   //   if (final) {
//   //     if (Update.end(true)) { // true to set the size to the current progress
//   //       Serial.printf("Update Success: %u\nRebooting...\n", index + len);
//   //     } else {
//   //       Update.printError(Serial);
//   //     }
//   //   } });
//   // Handle OTA Update
//   server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request)
//             {
//       // Respond with status after update
//       bool updateSuccess = !Update.hasError();
//       request->send(200, "text/plain", updateSuccess ? "Update successful!" : "Update failed!");
//       ESP.restart(); }, [](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
//             {
//       // Start OTA update process
//       if (!index) {
//         Serial.printf("Update Start: %s\n", filename.c_str());
//          size_t freeSketchSpace = ESP.getFreeSketchSpace(); // Dynamically get available space
//         // size_t freeSketchSpace=(ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
//         if (!Update.begin(freeSketchSpace)) { // Start with unknown size
//           Update.printError(Serial);
//         }
//       }

//       // Write OTA data
//       if (Update.write(data, len) != len) {
//         Update.printError(Serial);
//       }

//       // Finish OTA update
//       if (final) {
//         if (Update.end(true)) { // True to set as successful
//           Serial.printf("Update Success: %uB\n", index + len);
//         } else {
//           Update.printError(Serial);
//         }
//       } });

//   server.begin();
// }

// void loop()
// {
//   // digitalWrite(2, HIGH);
//   // delay(1000);
//   // digitalWrite(2, LOW);
//   // delay(1000);
// }

// Read and update website
// #include <ESP8266WiFi.h>
// #include <ESPAsyncWebServer.h>
// #include <ElegantOTA.h>
// #include <ESPAsyncTCP.h>
// #include <FS.h>
// #include "LittleFS.h"
// #include <Arduino_JSON.h>

// const char *ssid = "Nighthawks0230";
// const char *password = "liew1212";

// AsyncWebServer server(80);

// AsyncWebSocket ws("/ws");

// JSONVar readings;

// unsigned long lastTime = 0;
// unsigned long timerDelay = 1000;

// String getSensorReadings()
// {
//   readings['soil_moisture'] = String(analogRead(soilMoistureSensorPin));
// }

// unsigned long ota_progress_millis = 0;

// void onOTAStart()
// {
//   // Log when OTA has started
//   Serial.println("OTA update started!");
//   // <Add your own code here>
// }

// void onOTAProgress(size_t current, size_t final)
// {
//   // Log every 1 second
//   if (millis() - ota_progress_millis > 1000)
//   {
//     ota_progress_millis = millis();
//     Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
//   }
// }

// void onOTAEnd(bool success)
// {
//   // Log when OTA has finished
//   if (success)
//   {
//     Serial.println("OTA update finished successfully!");
//   }
//   else
//   {
//     Serial.println("There was an error during OTA update!");
//   }
//   // <Add your own code here>
// }

// String processor(const String &var)
// {
//   return String();
// }

// void setup(void)
// {

//   Serial.begin(115200);
//   WiFi.mode(WIFI_STA);
//   WiFi.begin(ssid, password);
//   Serial.println("");

//   if (!SPIFFS.begin())
//   {
//     Serial.println("An error has occured while mounting SPIFFS");
//     return;
//   }

//   // Wait for connection
//   while (WiFi.status() != WL_CONNECTED)
//   {
//     delay(500);
//     Serial.print(".");
//   }
//   Serial.println("");
//   Serial.print("Connected to ");
//   Serial.println(ssid);
//   Serial.print("IP address: ");
//   Serial.println(WiFi.localIP());

//   // server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
//   //   request->send(200, "text/plain", "Hi! This is ElegantOTA AsyncDemo.");
//   // });

//   server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
//             { request->send(SPIFFS, "/index.html", String(), false, processor); });

//   server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request)
//             { request->send(SPIFFS, "/style.css", "text/css"); });

//   ElegantOTA.begin(&server); // Start ElegantOTA
//   // ElegantOTA callbacks
//   ElegantOTA.onStart(onOTAStart);
//   ElegantOTA.onProgress(onOTAProgress);
//   ElegantOTA.onEnd(onOTAEnd);

//   server.begin();
//   Serial.println("HTTP server started");
// }

// void loop(void)
// {
//   ElegantOTA.loop();
// }
