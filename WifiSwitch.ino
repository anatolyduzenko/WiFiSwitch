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

// Default AP settings 
const char* apSSID = "WifiSwitch";
const char* apPassword = "12345678";
const char* defaultUsername = "admin";
const char* defaultPassword = "password";

// Device settings
#define EEPROM_SIZE 304

struct DeviceConfig {
  char wifiSSID[32]; // Wifi Network name
  char wifiPassword[64]; // Wifi password
  char deviceName[32]; // Device Name
  char deviceUser[16]; // Device Username
  char devicePassword[16]; // Device Password
  char apiServerIP[16]; // API Server IP
  char apiServerToken[64]; // API Server token
  char authToken[64]; // Authorization token
};

DeviceConfig config; // Create an instance of the config struct

bool load_on =  false;
bool can_toggle = false;
int button_state;


// Forms (Login and Settings)
const char* loginForm = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <title>ESP8266 Login</title>
    <style>
      body {
        font-family: Arial, sans-serif;
        background-color: #f4f4f9;
        margin: 0;
        padding: 0;
        display: flex;
        justify-content: center;
        align-items: center;
        height: 100vh;
      }
      .login-container {
        background: #fff;
        padding: 20px;
        border-radius: 8px;
        box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);
        width: 300px;
      }
      h2 {
        text-align: center;
      }
      input {
        width: 100%;
        padding: 10px;
        margin-bottom: 15px;
        border: 1px solid #ccc;
        border-radius: 5px;
      }
      button {
        width: 100%;
        padding: 10px;
        background-color: #007BFF;
        color: #fff;
        border: none;
        border-radius: 5px;
        cursor: pointer;
      }
      button:hover {
        background-color: #0056b3;
      }
    </style>
  </head>
  <body>
    <div class="login-container">
      <h2>Login</h2>
      <form method="POST" action="/login">
        <input type="text" name="username" placeholder="Username" required>
        <input type="password" name="password" placeholder="Password" required>
        <button type="submit">Login</button>
      </form>
    </div>
  </body>
  </html>
  )rawliteral";

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
              width: 95%;
              padding: 10px;
              margin-bottom: 15px;
              border: 1px solid #ccc;
              border-radius: 5px;
          }
          button {
              width: 100%;
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
              <label for='wifiSSID'>Wifi SSID</label>
              <input type='text' id='wifiSSID' name='wifiSSID' placeholder='Enter wireless network name' required>
              <label for='wifiPassword'>Wifi Password</label>
              <input type='password' id='wifiPassword' name='wifiPassword' placeholder='Enter password' required>
              <label for='deviceName'>Device Name</label>
              <input type='text' id='deviceName' name='deviceName' placeholder='Enter switch name'>
              <hr/>
              <label for='apiServerIP'>API Server IP</label>
              <input type='text' id='apiServerIP' name='apiServerIP' placeholder='Enter remote server IP'>
              <label for='apiServerToken'>API Server Token</label>
              <input type='text' id='apiServerToken' name='apiServerToken' placeholder='Enter remote server auth token'>
              <hr/>
              <label for='deviceUser'>Username</label>
              <input type='password' id='deviceUser' name='deviceUser' placeholder='Enter username' required>
              <label for='devicePassword'>Password</label>
              <input type='password' id='devicePassword' name='devicePassword' placeholder='Enter password' required>
              <label for='authToken'>Switch Auth Token</label>
              <input type='text' id='authToken' name='authToken' placeholder='Enter switch auth token'>
              <button type='submit'>Save</button>
          </form>
      </div>
  </body>
  </html>)rawliteral";


ESP8266WebServer server(80); // web-server
HTTPClient http; // web-client
WiFiClient wifi; // wifi

const int load = 0; // Set output pin
const int button = 2; // Set input switch


// Configuration functions 
// Initialize the struct with sample data
// void initializeConfig() {
//   strcpy(config.wifiSSID, "MyWiFi");
//   strcpy(config.wifiPassword, "12345678");
//   strcpy(config.apiServerIP, "192.168.1.100");
//   strcpy(config.apiServerToken, "abcdef1234567890");
//   strcpy(config.deviceName, "WifiSwitch");
//   strcpy(config.deviceUser, "admin");
//   strcpy(config.devicePassword, "password");
//   strcpy(config.authToken, "auth1234567890");
// }

// Save the struct to EEPROM
void saveConfig() {
  Serial.println("Saving configuration to EEPROM...");
  EEPROM.begin(EEPROM_SIZE);
  uint8_t* ptr = (uint8_t*)&config; // Pointer to the struct
  for (int i = 0; i < sizeof(config); i++) {
    EEPROM.write(i, *(ptr + i));
  }
  EEPROM.commit();
  Serial.println("Configuration saved!");
}

// Load the struct from EEPROM
bool loadConfig() {
  EEPROM.begin(EEPROM_SIZE);
  Serial.println("Loading configuration from EEPROM...");
  uint8_t* ptr = (uint8_t*)&config; // Pointer to the struct
  for (int i = 0; i < sizeof(config); i++) {
    *(ptr + i) = EEPROM.read(i);
  }

  EEPROM.end();
  // Validate by checking if `wifiName` is not empty
  return (config.wifiSSID[0] != '\0');
}


// Web management interface
// Authorization page
void handleRoot() { 
  server.send(200, "text/html", loginForm);
}

// Incorrect requests handler
void handleNotFound(){
  String message = "not found";
  server.send(404, "text/plain", message);
}

// Authorization handler
void handleLogin() {
  if (server.method() == HTTP_POST) {
    String username = server.arg("username");
    String password = server.arg("password");

    if (username == defaultUsername && password == defaultPassword) {
      server.sendHeader("Location", String("/settings"), true);
      server.send ( 302, "text/plain", "");
    } else {
      server.send(403, "text/html", "<h1>Login Failed!</h1><p>Invalid credentials. Please try again.</p><script type='text/javascript'>setTimeout(function(){ window.location = '/'; }, 200);</script>");
    }
  } else {
    server.send(405, "text/plain", "Method Not Allowed");
  }
}

// Settings form handler
void handleSettingsForm(){
  server.send(200, "text/html", settingsForm);
}

// Settings form saving handler
void handleSettingsSave() {
  if (server.method() == HTTP_POST) {
    
    String wifiSSID = server.arg("wifiName");
    String wifiPassword = server.arg("wifiPassword");
    String apiServerIP = server.arg("apiServerIP");
    String apiServerToken = server.arg("apiServerToken");
    String deviceName = server.arg("deviceName");
    String deviceUser = server.arg("deviceUSer");
    String devicePassword = server.arg("devicePassword");
    String authToken = server.arg("authToken");

    // Ensure no buffer overflows
    wifiSSID.toCharArray(config.wifiSSID, sizeof(config.wifiSSID));
    wifiPassword.toCharArray(config.wifiPassword, sizeof(config.wifiPassword));
    apiServerIP.toCharArray(config.apiServerIP, sizeof(config.apiServerIP));
    apiServerToken.toCharArray(config.apiServerToken, sizeof(config.apiServerToken));
    deviceName.toCharArray(config.deviceName, sizeof(config.deviceName));
    deviceUser.toCharArray(config.deviceUser, sizeof(config.deviceUser));
    devicePassword.toCharArray(config.devicePassword, sizeof(config.devicePassword));
    authToken.toCharArray(config.authToken, sizeof(config.authToken));

    // Save the updated configuration to EEPROM
    saveConfig();

    // Send response back to the client
    server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Configuration saved! Restart the device.\"}");
  } else {
    server.send(405, "application/json", "{\"status\":\"error\",\"message\":\"Method not allowed!\"}");
  }
}

// API Server status update 
void sendServer(bool state){
  http.begin(wifi, "http://"+String(config.apiServerIP)+"/iapi/setstate");
  String post = "token="+String(config.apiServerToken)+"&state="+(state?"on":"off"); // The API server will determine the device by token (ToDO)
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int httpCode = http.POST(post);
  http.end();  
}

// Server load handlers
void handleOn(){
  String token = server.arg("token");
  if(!token.equals(config.authToken)) {
    String message = "access denied";
    server.send(401, "text/plain", message);
    return;
  }
  turnLoadOn();
  String message = "success";
  server.send(200, "text/plain", message);
}

void handleOff(){
  String token = server.arg("token");
  if(!token.equals(config.authToken)) {
    String message = "access denied";
    server.send(401, "text/plain", message);
    return;
  }
  turnLoadOff();
  String message = "success";
  server.send(200, "text/plain", message);
}


// Manipulations with the load
// Switch On
void turnLoadOn(){
  digitalWrite(load, LOW);
  load_on = true;
}

// Switch Off
void turnLoadOff(){
  digitalWrite(load, HIGH);
  load_on = false;
}

// State toggle
void toggleLoadState(){
  if(load_on == true) {
    turnLoadOff();
    sendServer(false);
  } else {
    turnLoadOn();
    sendServer(true);
  }
}



void initVariant() {
  uint8_t mac[6] = {0x00, 0xA3, 0xA0, 0x1C, 0x8C, 0x45};
  wifi_set_macaddr(STATION_IF, &mac[0]);
}

void setup(void){
  pinMode(load, OUTPUT);
  pinMode(button, INPUT_PULLUP); // Important to make INPUT_PULLUP
  turnLoadOff();
  
  if (loadConfig()) {
    WiFi.mode(WIFI_STA);
    WiFi.hostname(config.deviceName);
    WiFi.begin(config.wifiSSID, config.wifiPassword);

    // Waiting for WiFi connection
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      attempts++;
    }
  } 
  
  if(WiFi.status() != WL_CONNECTED)  {
    WiFi.mode(WIFI_AP); // Set to Access Point mode
    WiFi.softAP(apSSID, apPassword);
  }
  
  // Configure routes
  server.on("/", handleRoot);
  server.on("/on",  HTTP_POST, handleOn);
  server.on("/off", HTTP_POST, handleOff);
  server.on("/login", HTTP_POST, handleLogin);
  server.on("/settings", HTTP_GET, handleSettingsForm);
  server.on("/settings", HTTP_POST, handleSettingsSave);
  server.onNotFound(handleNotFound);
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

  button_state = digitalRead(button);
  if (button_state == HIGH && can_toggle) {
    toggleLoadState();
    can_toggle = false;
    delay(500);
  } else if(button_state == LOW){
    can_toggle = true;
  }
}