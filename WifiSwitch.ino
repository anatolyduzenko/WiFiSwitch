#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
extern "C" {
  #include "user_interface.h"
}

#define EEPROM_SIZE 96  // Size of EEPROM to store SSID and Password

char ssid[32];       // Buffer for SSID (max length 32)
char password[64];   // Buffer for Password (max length 64)
const char* apSSID = "WifiSwitch";
const char* apPassword = "12345678";
const String self_token = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"; // Token
const String serv_token = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"; // Server Token
const String name = "WifiSwitch"; // Switch Name
const String serverIP = "192.168.0.55"; // IP WEB 
bool lamp_on =  false;
bool can_toggle = false;
int button_state;
const char settingsForm[] PROGMEM = R"rawliteral(
  <!DOCTYPE html>
  <html lang='en'>
  <head>
      <meta charset='UTF-8'>
      <meta name='viewport' content='width=device-width, initial-scale=1.0'>
      <title>Wifi Switcher Settings</title>
      <style>
          body {
              font-family: Arial, sans-serif;
              margin: 0;
              padding: 0;
              background-color: #f4f4f9;
          }
          .container {
              width: 100%;
              max-width: 400px;
              margin: 50px auto;
              padding: 20px;
              background: #fff;
              box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);
              border-radius: 8px;
          }
          h2 {
              margin-bottom: 20px;
              text-align: center;
              color: #333;
          }
          label {
              font-weight: bold;
              display: block;
              margin-bottom: 5px;
          }
          input, button {
              width: 100%;
              padding: 10px;
              margin-bottom: 15px;
              border: 1px solid #ccc;
              border-radius: 5px;
          }
          button {
              background-color: #007BFF;
              color: #fff;
              border: none;
              cursor: pointer;
              font-size: 16px;
          }
          button:hover {
              background-color: #0056b3;
          }
      </style>
  </head>
  <body>
      <div class='container'>
          <h2>WifiSwitcher Settings</h2>
          <form action='/settings' method='POST'>
              <label for='ssid'>Wifi SSID</label>
              <input type='text' id='ssid' name='ssid' placeholder='Enter network name' required>

              <label for='password'>Wifi PW</label>
              <input type='password' id='password' name='password' placeholder='Enter password' required>

              <button type='submit'>Save</button>
          </form>
      </div>
  </body>
  </html>)rawliteral";


ESP8266WebServer server(80); // web-server
HTTPClient http; // web-client
WiFiClient wifi;

const int lamp = 0; // Set output
const int button = 2; // manual switch

// Function to save Wi-Fi credentials to EEPROM
void saveWiFiCredentials(const char* ssid, const char* password) {
  EEPROM.begin(EEPROM_SIZE);  // Initialize EEPROM
  for (int i = 0; i < 32; ++i) {
    EEPROM.write(i, ssid[i]); // Write SSID
  }
  for (int i = 0; i < 64; ++i) {
    EEPROM.write(32 + i, password[i]); // Write Password
  }
  EEPROM.commit();  // Save changes
  Serial.println("Wi-Fi Credentials Saved to EEPROM");
}

// Function to load Wi-Fi credentials from EEPROM
bool loadWiFiCredentials(char* ssid, char* password) {
  EEPROM.begin(EEPROM_SIZE);
  bool valid = false;
  for (int i = 0; i < 32; ++i) {
    ssid[i] = EEPROM.read(i);
    if (ssid[i] != 0xFF) valid = true; // Check for empty EEPROM
  }
  for (int i = 0; i < 64; ++i) {
    password[i] = EEPROM.read(32 + i);
  }
  ssid[31] = '\0';  // Ensure null-termination
  password[63] = '\0';
  EEPROM.end();
  return valid; // Return if valid credentials exist
}

// Internal web-server check
void handleRoot() { 
  server.send(200, "text/plain", "Hello! I am " + name);
}

// Incorrect requests handler
void handleNotFound(){
  String message = "not found";
  server.send(404, "text/plain", message);
}

// Switch On
void turnOnLamp(){
  digitalWrite(lamp, LOW);
  lamp_on = true;
}

// Switch Off
void turnOffLamp(){
  digitalWrite(lamp, HIGH);
  lamp_on = false;
}

// Server update
void sendServer(bool state){
  http.begin(wifi, "http://"+serverIP+"/iapi/setstate");
  String post = "token="+self_token+"&state="+(state?"on":"off"); // По токену сервер будет определять что это за устройство
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int httpCode = http.POST(post);
  http.end();  
}

// State toggle
void toggleLamp(){
  if(lamp_on == true) {
    turnOffLamp();
  //  sendServer(false);
  } else {
    turnOnLamp();
  //  sendServer(true);
  }
}

// Server handlers
void handleOn(){
  String token = server.arg("token");
  if(serv_token != token) {
    String message = "access denied";
    server.send(401, "text/plain", message);
    return;
  }
  turnOnLamp();
  String message = "success";
  server.send(200, "text/plain", message);
}

void handleOff(){
  String token = server.arg("token");
  if(serv_token != token) {
    String message = "access denied";
    server.send(401, "text/plain", message);
    return;
  }
  turnOffLamp();
  String message = "success";
  server.send(200, "text/plain", message);
}

void handleSettingsForm(){
  server.send(200, "text/html", settingsForm);
}

void handleSaveSettings() {
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  saveWiFiCredentials(ssid.c_str(), password.c_str());
  server.send(200, "text/html", "Saved, restart the device!");
}

void initVariant() {
  uint8_t mac[6] = {0x00, 0xA3, 0xA0, 0x1C, 0x8C, 0x45};
  wifi_set_macaddr(STATION_IF, &mac[0]);
}

void setup(void){
  pinMode(lamp, OUTPUT);
  pinMode(button, INPUT_PULLUP); // Важно сделать INPUT_PULLUP
  turnOffLamp();
  WiFi.mode(WIFI_STA);
  if (loadWiFiCredentials(ssid, password) == true) {
    WiFi.hostname(name);
    WiFi.begin(ssid, password);

    // Ждем пока подключимся к WiFi
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      attempts++;
    }
  } 
  
  WiFi.mode(WIFI_AP); // Set to Access Point mode
  WiFi.softAP(apSSID, apPassword);
  
  // Назначем функции на запросы
  server.on("/", handleRoot);
  server.on("/on",  HTTP_POST, handleOn);
  server.on("/off", HTTP_POST, handleOff);
  server.on("/settings", HTTP_GET, handleSettingsForm);
  server.on("/settings", HTTP_POST, handleSaveSettings);
  server.onNotFound(handleNotFound);

  // Стартуем сервер
  server.begin();

  // OTA Setup
  ArduinoOTA.onStart([]() {
    Serial.println("OTA Update Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA Update End");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error [%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  Serial.println("OTA Ready");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void loop(void){
  ArduinoOTA.handle();
  server.handleClient();

  // Проверяем нажатие выключателя
  button_state = digitalRead(button);
  if (button_state == HIGH && can_toggle) {
    toggleLamp();
    can_toggle = false;
    delay(500);
  } else if(button_state == LOW){
    can_toggle = true;
  }
}