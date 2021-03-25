/*********
  Rui Santos
  Complete instructions at https://RandomNerdTutorials.com/esp32-status-indicator-sensor-pcb/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "SPIFFS.h"
#include <Arduino_JSON.h>
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create an Event Source on /events
AsyncEventSource events("/events");

// Search for parameter in HTTP POST request
const char* PARAM_INPUT_1 = "ssid";
const char* PARAM_INPUT_2 = "pass";
const char* PARAM_INPUT_3 = "ip";

//Variables to save values from HTML form
String ssid;
String pass;
String ip;

// File paths to save input values permanently
const char* ssidPath = "/ssid.txt";
const char* passPath = "/pass.txt";
const char* ipPath = "/ip.txt";

IPAddress localIP;
//IPAddress localIP(192, 168, 1, 200); // hardcoded

// Set your Gateway IP address
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 0, 0);

// Timer variables (check wifi)
unsigned long previousMillis = 0;
const long interval = 10000;  // interval to wait for Wi-Fi connection (milliseconds)

// WS2812B Addressable RGB LEDs
#define STRIP_1_PIN    27  // GPIO the LEDs are connected to
#define STRIP_2_PIN    32  // GPIO the LEDs are connected to
#define LED_COUNT  5  // Number of LEDs
#define BRIGHTNESS 50  // NeoPixel brightness, 0 (min) to 255 (max)
Adafruit_NeoPixel strip1(LED_COUNT, STRIP_1_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip2(LED_COUNT, STRIP_2_PIN, NEO_GRB + NEO_KHZ800);

// Create a sensor object
Adafruit_BME280 bme;         // BME280 connect to ESP32 I2C (GPIO 21 = SDA, GPIO 22 = SCL)

//Variables to hold sensor readings
float temp;
float hum;
float pres;

// Json Variable to Hold Sensor Readings
JSONVar readings;

// Timer variables (get sensor readings)
unsigned long lastTime = 0;
unsigned long timerDelay = 30000;

//-----------------FUNCTIONS TO HANDLE SENSOR READINGS-----------------//

// Init BME280
void initBME(){
  if (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }
}

// Get Sensor Readings
void getSensorReadings(){
  temp = bme.readTemperature();
  hum = bme.readHumidity();
  pres= bme.readPressure()/100.0F;
}

// Return JSON String from sensor Readings
String getJSONReadings(){
  readings["temperature"] = String(temp);
  readings["humidity"] =  String(hum);
  readings["pressure"] = String(pres);
  String jsonString = JSON.stringify(readings);
  return jsonString;
}

//Update RGB LED colors accordingly to temp and hum values
void updateColors(){
  strip1.clear();
  strip2.clear();

  //Number of lit LEDs (temperature)
  int tempLEDs;
  if (temp<=0){
    tempLEDs = 1;
  }
  else if (temp>0 && temp<=10){
    tempLEDs = 2;
  }
  else if (temp>10 && temp<=20){
    tempLEDs = 3;
  }
  else if (temp>20 && temp<=30){
    tempLEDs = 4;
  }
  else{
    tempLEDs = 5;
  }

  //Turn on LEDs for temperature
  for(int i=0; i<tempLEDs; i++) {
    strip1.setPixelColor(i, strip1.Color(255, 165, 0));
    strip1.show();   
  }
  
  //Number of lit LEDs (humidity)
  int humLEDs = map(hum, 0, 100, 1, LED_COUNT);

  //Turn on LEDs for humidity
  for(int i=0; i<humLEDs; i++) { // For each pixel...
    strip2.setPixelColor(i, strip2.Color(25, 140, 200));
    strip2.show();
  }
}

//-----------------FUNCTIONS TO HANDLE SPIFFS AND FILES-----------------//

// Initialize SPIFFS
void initSPIFFS() {
  if (!SPIFFS.begin(true)) {
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  else{
    Serial.println("SPIFFS mounted successfully");
  }
}

// Read File from SPIFFS
String readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path);
  if(!file || file.isDirectory()){
    Serial.println("- failed to open file for reading");
    return String();
  }
  
  String fileContent;
  while(file.available()){
    fileContent = file.readStringUntil('\n');
    break;     
  }
  return fileContent;
}

// Write file to SPIFFS
void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\r\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("- failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("- file written");
  } else {
    Serial.println("- frite failed");
  }
}

// Initialize WiFi
bool initWiFi() {
  if(ssid=="" || ip==""){
    Serial.println("Undefined SSID or IP address.");
    return false;
  }

  WiFi.mode(WIFI_STA);
  localIP.fromString(ip.c_str());

  if (!WiFi.config(localIP, gateway, subnet)){
    Serial.println("STA Failed to configure");
    return false;
  }
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.println("Connecting to WiFi...");

  unsigned long currentMillis = millis();
  previousMillis = currentMillis;

  while(WiFi.status() != WL_CONNECTED) {
    currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      Serial.println("Failed to connect.");
      return false;
    }
  }

  Serial.println(WiFi.localIP());
  return true;
}

void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);
  
  // Initialize strips
  strip1.begin();
  strip2.begin();
  
  // Set brightness 
  strip1.setBrightness(BRIGHTNESS);
  strip2.setBrightness(BRIGHTNESS);
  
  // Init BME280 senspr
  initBME();
  
  // Init SPIFFS
  initSPIFFS();

  // Load values saved in SPIFFS
  ssid = readFile(SPIFFS, ssidPath);
  pass = readFile(SPIFFS, passPath);
  ip = readFile(SPIFFS, ipPath);
  /*Serial.println(ssid);
  Serial.println(pass);
  Serial.println(ip);*/

  if(initWiFi()) {
    // If ESP32 inits successfully in station mode light up all pixels in a teal color
    for(int i=0; i<LED_COUNT; i++) { // For each pixel...
      strip1.setPixelColor(i, strip1.Color(0, 255, 128));
      strip2.setPixelColor(i, strip2.Color(0, 255, 128));

      strip1.show();   // Send the updated pixel colors to the hardware.
      strip2.show();   // Send the updated pixel colors to the hardware.
    }

    //Handle the Web Server in Station Mode
    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(SPIFFS, "/index.html", "text/html");
    });
    server.serveStatic("/", SPIFFS, "/");

    // Request for the latest sensor readings
    server.on("/readings", HTTP_GET, [](AsyncWebServerRequest *request){
      getSensorReadings();
      String json = getJSONReadings();
      request->send(200, "application/json", json);
      json = String();
    });

    events.onConnect([](AsyncEventSourceClient *client){
      if(client->lastId()){
        Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
      }
    });
    server.addHandler(&events);
    
    server.begin();
  }
  else {
    // else initialize the ESP32 in Access Point mode
    // light up all pixels in a red color
    for(int i=0; i<LED_COUNT; i++) { // For each pixel...
      strip1.setPixelColor(i, strip1.Color(255, 0, 0));
      strip2.setPixelColor(i, strip2.Color(255, 0, 0));
      //strip1.setPixelColor(i, strip1.Color(128, 0, 21));
      //strip2.setPixelColor(i, strip2.Color(128, 0, 21));

      strip1.show();   // Send the updated pixel colors to the hardware.
      strip2.show();   // Send the updated pixel colors to the hardware.
    }

    // Set Access Point
    Serial.println("Setting AP (Access Point)");
    // NULL sets an open Access Point
    WiFi.softAP("ESP-WIFI-MANAGER", NULL);

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP); 

    // Web Server Root URL For WiFi Manager Web Page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(SPIFFS, "/wifimanager.html", "text/html");
    });
    
    server.serveStatic("/", SPIFFS, "/");
    
    // Get the parameters submited on the form 
    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
      int params = request->params();
      for(int i=0;i<params;i++){
        AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()){
          // HTTP POST ssid value
          if (p->name() == PARAM_INPUT_1) {
            ssid = p->value().c_str();
            Serial.print("SSID set to: ");
            Serial.println(ssid);
            // Write file to save value
            writeFile(SPIFFS, ssidPath, ssid.c_str());
          }
          // HTTP POST pass value
          if (p->name() == PARAM_INPUT_2) {
            pass = p->value().c_str();
            Serial.print("Password set to: ");
            Serial.println(pass);
            // Write file to save value
            writeFile(SPIFFS, passPath, pass.c_str());
          }
          // HTTP POST ip value
          if (p->name() == PARAM_INPUT_3) {
            ip = p->value().c_str();
            Serial.print("IP Address set to: ");
            Serial.println(ip);
            // Write file to save value
            writeFile(SPIFFS, ipPath, ip.c_str());
          }
          //Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
        }
      }
      request->send(200, "text/plain", "Done. ESP will restart, connect to your router and go to IP address: " + ip);
      delay(3000);
      // After saving the parameters, restart the ESP32
      ESP.restart();
    });
    server.begin();
  }
}

void loop() {
  // If the ESP32 is set successfully in station mode...
  if (WiFi.status() == WL_CONNECTED) {

    //...Send Events to the client with sensor readins and update colors every 30 seconds
    if (millis() - lastTime > timerDelay) {
      getSensorReadings();
      updateColors();
      
      String message = getJSONReadings();
      events.send(message.c_str(),"new_readings" ,millis());
      lastTime = millis();
    }
  }
}
