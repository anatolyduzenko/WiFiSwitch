#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
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
#define EEPROM_SIZE 512

// Tokens
String generateToken(int length) {
  const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  String token = "";

  for (int i = 0; i < length; i++) {
    int index = random(0, sizeof(charset) - 1);
    token += charset[index];
  }

  return token;
}

struct DeviceConfig {
  char wifiSSID[32]; // Wifi Network name
  char wifiPassword[64]; // Wifi password
  uint8_t wifiMAC[6]; // Wifi MAC Address
  char deviceName[32]; // Device Name
  char deviceUser[16]; // Device Username
  char devicePassword[16]; // Device Password
  char apiServerIP[16]; // API Server IP
  char apiServerEndpoint[32]; // API Server Endpoint
  char apiServerToken[64]; // API Server token
  char authToken[64] = "wifiSW_01"; // Authorization token
};

DeviceConfig config; // Create an instance of the config struct

bool load_on =  false;
bool can_toggle = false;
int button_state;


// Forms (Login and Settings)
const char* switchForm = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <meta charset='UTF-8'>
    <meta name='viewport' content='width=device-width, initial-scale=1.0'>
    <title>Wifi Switch</title>
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
      .switch-container {
        text-align: center;
        background-color: rgba(255, 255, 255, 0.9);
        padding: 20px;
        border-radius: 8px;
        box-shadow: 0 0 10px rgba(0, 0, 0, 0.2);
      }
      button {
        width: 100px;
        padding: 10px;
        margin: 10px;
        background-color: #007BFF;
        color: #fff;
        border: none;
        border-radius: 5px;
        cursor: pointer;
      }
      div.img {
        border: 3px solid #183186;
        height: 175px;
        border-radius: 10px;
        box-shadow: aquamarine;
        background-image: url(data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAzODQgNTEyIj48cGF0aCBmaWxsPSIjNjNFNkJFIiBkPSJNMjk3LjIgMjQ4LjlDMzExLjYgMjI4LjMgMzIwIDIwMy4yIDMyMCAxNzZjMC03MC43LTU3LjMtMTI4LTEyOC0xMjhTNjQgMTA1LjMgNjQgMTc2YzAgMjcuMiA4LjQgNTIuMyAyMi44IDcyLjljMy43IDUuMyA4LjEgMTEuMyAxMi44IDE3LjdjMCAwIDAgMCAwIDBjMTIuOSAxNy43IDI4LjMgMzguOSAzOS44IDU5LjhjMTAuNCAxOSAxNS43IDM4LjggMTguMyA1Ny41TDEwOSAzODRjLTIuMi0xMi01LjktMjMuNy0xMS44LTM0LjVjLTkuOS0xOC0yMi4yLTM0LjktMzQuNS01MS44YzAgMCAwIDAgMCAwczAgMCAwIDBzMC0wIDAgMGMtNS4yLTcuMS0xMC40LTE0LjItMTUuNC0yMS40QzI3LjYgMjQ3LjkgMTYgMjEzLjMgMTYgMTc2QzE2IDc4LjggOTQuOCAwIDE5MiAwczE3NiA3OC44IDE3NiAxNzZjMCAzNy4zLTExLjYgNzEuOS0zMS40IDEwMC4zYy01IDcuMi0xMC4yIDE0LjMtMTUuNCAyMS40YzAgMCAwIDAgMCAwczAgMCAwIDBjLTEyLjMgMTYuOC0yNC42IDMzLjctMzQuNSA1MS44Yy01LjkgMTAuOC05LjYgMjIuNS0xMS44IDM0LjVsLTQ4LjYgMGMyLjYtMTguNyA3LjktMzguNiAxOC4zLTU3LjVjMTEuNS0yMC45IDI2LjktNDIuMSAzOS44LTU5LjhjMCAwIDAgMCAwIDBzMC0wIDAgMGM0LjctNi40IDktMTIuNCAxMi43LTE3Ljd6TTE5MiAxMjhjLTI2LjUgMC00OCAyMS41LTQ4IDQ4YzAgOC44LTcuMiAxNi0xNiAxNnMtMTYtNy4yLTE2LTE2YzAtNDQuMiAzNS44LTgwIDgwLTgwYzguOCAwIDE2IDcuMiAxNiAxNnMtNy4yIDE2LTE2IDE2em0wIDM4NGMtNDQuMiAwLTgwLTM1LjgtODAtODBsMC0xNiAxNjAgMCAwIDE2YzAgNDQuMi0zNS44IDgwLTgwIDgweiIvPjwvc3ZnPg==);
        background-size: 100px;
        background-position: center;
        background-repeat: no-repeat;
      }
      .off{
        background-color: #183186;
      }
      button:hover {
        background-color: #0056b3;
      }
      a {
        display: block;
        margin-top: 20px;
        color: #007BFF;
        text-decoration: none;
      }
      a:hover {
        text-decoration: underline;
      }
    </style>
  </head>
  <body>
    <div class='switch-container'>
      <h2>Switch Control</h2>
      <form>
        <div id='statusBulb' class='img {STATUS}'></div>
        <button type='button' onclick='switchOn()'>On</button>
        <button type='button' onclick='switchOff()'>Off</button>
      </form>
      <a href='/login'>Settings</a>
      <script>
        const authToken = "{TOKEN}";
        function switchOn() {
          fetch('/on', { 
              method: 'POST', 
              headers: {
                'Authorization': authToken
              } 
            })
            .then(response => {
              if (response.ok) {
                const el = document.getElementById('statusBulb');
                el.classList.remove("off");
                el.classList.add("on");
              } else {
                alert('Failed to turn ON the switch.');
              }
            })
            .catch(error => {
              alert('Error: ' + error.message);
            });
        }
        function switchOff() {
          fetch('/off', { 
              method: 'POST', 
              headers: {
                'Authorization': authToken
              } 
            })
            .then(response => {
              if (response.ok) {
                const el = document.getElementById('statusBulb');
                el.classList.remove("on");
                el.classList.add("off");
              } else {
                alert('Failed to turn OFF the switch.');
              }
            })
            .catch(error => {
              alert('Error: ' + error.message);
            });
        }
      </script>
    </div>
  </body>
  </html>
  )rawliteral";

const char* loginForm = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <meta charset='UTF-8'>
    <meta name='viewport' content='width=device-width, initial-scale=1.0'>
    <title>Wifi Switch Login</title>
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
        width: 95%;
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
      <form id="loginForm">
        <label for="username">Username:</label><br>
        <input type="text" id="username" name="username"><br><br>
        <label for="password">Password:</label><br>
        <input type="password" id="password" name="password"><br><br>
        <button type="button" onclick="authenticate()">Login</button>
      </form>
      <script>
        function authenticate() {
          const username = document.getElementById('username').value;
          const password = document.getElementById('password').value;

          fetch('/login', {
            method: 'POST',
            headers: {
              'Content-Type': 'application/json'
            },
            body: JSON.stringify({ username, password })
          })
          .then(response => {
            if (response.status === 200) {
              return response.json();
            } else {
              throw new Error('Authentication failed.');
            }
          })
          .then(data => {
            console.log('Authentication successful! Token: ' + data.token);
            window.location = '/settings';
            localStorage.setItem('authToken', data.token);
          })
          .catch(error => {
            alert(error.message);
          });
        }
    </script>
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
      <title>Wifi Switch Settings</title>
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
          width: 94%;
          padding: 10px;
          margin-bottom: 15px;
          border: 1px solid #ccc;
          border-radius: 5px;
        }
        input[type=checkbox] {
          width: auto;
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
        button.danger {
          background-color: red;
          color: #fff;
        }
        button.danger:hover {
          background-color: #ad0000;
        }
        a {
          display: block;
          margin-top: 5px;
          margin-bottom: 10px;
          color: #007BFF;
          text-decoration: none;
        }
        a:hover {
          text-decoration: underline;
        }
        .inline {
          display:inline;
        }
        .right {
          float:right;
        }
      </style>
      <script>
        function loadSettings() {
          const authToken = "{TOKEN}"; 

          fetch('/settings/load', {
            method: 'GET',
            headers: {
              'Authorization': authToken
            }
          })
          .then(response => response.json())
          .then(data => {
            document.getElementById('wifiSSID').value = data.wifiSSID;
            document.getElementById('wifiPassword').value = data.wifiPassword;
            document.getElementById('wifiMAC').value = data.wifiMAC;
            document.getElementById('apiServerIP').value = data.apiServerIP;
            document.getElementById('apiServerEndpoint').value = data.apiServerEndpoint;
            document.getElementById('apiServerToken').value = data.apiServerToken;
            document.getElementById('deviceName').value = data.deviceName;
            document.getElementById('deviceUser').value = data.deviceUser;
            document.getElementById('devicePassword').value = data.devicePassword;
            document.getElementById('authToken').value = data.authToken;
          })
          .catch(error => console.error('Error loading settings:', error));
        }

        function ToggleWifiPassword() {
          var x = document.getElementById("wifiPassword");
          if (x.type === "password") {
            x.type = "text";
          } else {
            x.type = "password";
          }
        }

        function ToggleDevicePassword() {
          var x = document.getElementById("devicePassword");
          if (x.type === "password") {
            x.type = "text";
          } else {
            x.type = "password";
          }
        }

        function saveSettings() {
          const wifiSSID = document.getElementById('wifiSSID').value;
          const wifiPassword = document.getElementById('wifiPassword').value;
          const wifiMAC = document.getElementById('wifiMAC').value;
          const deviceName = document.getElementById('deviceName').value;
          const deviceUser = document.getElementById('deviceUser').value;
          const devicePassword = document.getElementById('devicePassword').value;
          const apiServerIP = document.getElementById('apiServerIP').value;
          const apiServerEndpoint = document.getElementById('apiServerEndpoint').value;
          const apiServerToken = document.getElementById('apiServerToken').value;
          const authToken = document.getElementById('authToken').value;

          fetch('/settings', {
            method: 'POST',
            headers: {
                 'Content-Type': 'application/json'
            },
            body: JSON.stringify({ 
              wifiSSID, wifiPassword, wifiMAC,
              deviceName, deviceUser, devicePassword,
              apiServerIP, apiServerEndpoint, apiServerToken,
              authToken
             })
          })
          .then(response => {
            if (response.status === 200) {
              alert('Settings saved!');
              return response.json();
            } else {
              throw new Error('Authentication failed.');
            }
          })
          .then(data => {
            window.location = '/settings';
          })
          .catch(error => {
            alert(error.message);
          });
        }

        function generateToken() {
          fetch('/settings/token', {
            method: 'GET'
          })
          .then(response => response.json())
          .then(data => {
            document.getElementById('authToken').value = data.authToken;
          })
          .catch(error => console.error('Error creating token:', error));
        }

        window.onload = loadSettings;
      </script>
  </head>
  <body>
    <div class='container'>
      <h2>Wifi Switch Settings</h2>
      <a href='/'>Back to main screen</a>
      <form id='settingsForm' action='/settings' method='POST'>
        <label for='wifiSSID'>Wifi SSID</label>
        <input type='text' id='wifiSSID' name='wifiSSID' placeholder='Enter wireless network name' required>
        <label for='wifiPassword' class='inline'>Wifi Password</label>
        <label for='showPW' class='inline right'>Show</label>
        <input id='showPW' class='right' type='checkbox' onclick='ToggleWifiPassword()'>
        <input type='password' id='wifiPassword' name='wifiPassword' placeholder='Enter password' required>
        <label for='wifiMAC'>Wifi MAC Address</label>
        <input type='text' id='wifiMAC' name='wifiMAC' placeholder='Enter device MAC Address' required>
        <hr/>
        <label for='deviceName'>Device Name</label>
        <input type='text' id='deviceName' name='deviceName' placeholder='Enter switch name'>
        <label for='deviceUser'>Username</label>
        <input type='text' id='deviceUser' name='deviceUser' placeholder='Enter username'>
        <label for='devicePassword' class='inline'>Password</label>
        <label for='showDPW' class='inline right'>Show</label>
        <input id='showDPW' class='right' type='checkbox' onclick='ToggleDevicePassword()'>
        <input type='password' id='devicePassword' name='devicePassword' placeholder='Enter password'>
        <label for='authToken'>Authorization Token</label>
        <input type='text' id='authToken' name='authToken' placeholder='Enter authorization token'>
        <a href='#' onclick='generateToken()'>Generate token</a>
        <hr/>
        <label for='apiServerIP'>API Server IP</label>
        <input type='text' id='apiServerIP' name='apiServerIP' placeholder='Enter remote server IP'>
        <label for='apiServerEndpoint'>API Server Endpoint</label>
        <input type='text' id='apiServerEndpoint' name='apiServerEndpoint' placeholder='Enter remote server endpoint'>
        <label for='apiServerToken'>API Server Token</label>
        <input type='text' id='apiServerToken' name='apiServerToken' placeholder='Enter remote server auth token'>
        <hr/>
        <button type='button' onclick='saveSettings()'>Save</button>
      </form>
      <form action='/restart' method='POST'>
        <input type='hidden' name='token' value='{TOKEN}' />
        <button class='danger' type='submit' >Restart</button>
      </form>
    </div>
  </body>
  </html>)rawliteral";


ESP8266WebServer server(80); // web-server
HTTPClient http; // web-client
WiFiClient wifi; // wifi

const int load = 0; // Set output pin
const int button = 2; // Set input switch

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
  // Validate by checking if `wifiSSID` is not empty
  return (config.wifiSSID[0] != 0xFF);
}

// get Config as JSON 
String getConfigAsJson() {
  JsonDocument jsonDoc;
  jsonDoc["wifiSSID"] = config.wifiSSID;
  jsonDoc["wifiPassword"] = config.wifiPassword;
  jsonDoc["wifiMAC"] = macToString(config.wifiMAC);
  jsonDoc["apiServerIP"] = config.apiServerIP;
  jsonDoc["apiServerEndpoint"] = config.apiServerEndpoint;
  jsonDoc["apiServerToken"] = config.apiServerToken;
  jsonDoc["deviceName"] = config.deviceName;
  jsonDoc["deviceUser"] = config.deviceUser;
  jsonDoc["devicePassword"] = config.devicePassword;
  jsonDoc["authToken"] = config.authToken;

  String jsonString;
  serializeJson(jsonDoc, jsonString);
  return jsonString;
}

// Debug function used to print config
void printConfig() {
  Serial.println("Device Configuration:");
  Serial.print("Wi-Fi Name: ");
  Serial.println(config.wifiSSID);
  Serial.print("Wi-Fi Pass: ");
  Serial.println(config.wifiPassword);
  Serial.print("Device Name: ");
  Serial.println(config.deviceName);
  Serial.print("Device User: ");
  Serial.println(config.deviceUser);
  Serial.print("Device Pass: ");
  Serial.println(config.devicePassword);
  Serial.print("API Server IP: ");
  Serial.println(config.apiServerIP);
  Serial.print("API Server Endpoint: ");
  Serial.println(config.apiServerEndpoint);
  Serial.print("API Server Token: ");
  Serial.println(config.apiServerToken);
  Serial.print("Auth Token: ");
  Serial.println(config.authToken);
}

// convert MAC string to array
bool parseMAC(const String &macStr, uint8_t mac[6]) {
  if (macStr.length() != 17) { 
    return false;
  }

  int values[6];
  if (sscanf(macStr.c_str(), "%x:%x:%x:%x:%x:%x", 
             &values[0], &values[1], &values[2], 
             &values[3], &values[4], &values[5]) == 6) {
    for (int i = 0; i < 6; i++) {
      mac[i] = (uint8_t)values[i];
    }
    return true;
  }
  return false;
}

//convert MAC array to string
String macToString(uint8_t mac[6]) {
  char macStr[18]; // Buffer for "XX:XX:XX:XX:XX:XX" format
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macStr);
}

// validate MAC
bool validateMAC(uint8_t mac[6]) {
  for (int i = 0; i < 6; i++) {
    if (mac[i] < 0x00 || mac[i] > 0xFF) {
      return false; // Invalid byte found
    }
  }
  return true; // MAC is valid
}

// Web management interface
// Authorization page
void handleRoot() { 
  String html = switchForm; 
  html.replace("{TOKEN}", config.authToken); 
  if(load_on == true) {
    html.replace("{STATUS}", "on"); 
  } else {
    html.replace("{STATUS}", "off"); 
  }
  server.send(200, "text/html", html);
}

// Authorization page
void handleLoginForm() { 
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
    String payload = server.arg("plain");
    JsonDocument doc;
    deserializeJson(doc, payload);

    String username = doc["username"];
    String password = doc["password"];
    if ((username.equals(config.deviceUser) && password.equals(config.devicePassword)) || (username.equals(defaultUsername) && password.equals(defaultPassword))) {
      server.send(200, "application/json", "{\"token\":\"" + String(config.authToken) + "\"}");
    } else {
      server.send(403, "text/html", "<h1>Login Failed!</h1><p>Invalid credentials. Please try again.</p><script type='text/javascript'>setTimeout(function(){ window.location = '/'; }, 200);</script>");
    }
  } else {
    server.send(405, "text/json", "{\"status\":\"success\",\"message\":\"Method Not Allowed\"}");
  }
}

// Settings form handler
void handleSettingsForm(){
  //String token = server.arg("token");
  //if(!token.equals(config.authToken)) {
    String html = settingsForm; 
    html.replace("{TOKEN}", config.authToken); 
    server.send(200, "text/html", html);
  //}
}

// Settings form saving handler
void handleSettingsSave() {
  if (server.method() == HTTP_POST) {
    
    String payload = server.arg("plain");
    JsonDocument doc;
    deserializeJson(doc, payload);

    String wifiSSID = doc["wifiSSID"];
    String wifiPassword = doc["wifiPassword"];
    String wifiMAC = doc["wifiMAC"];
    String apiServerIP = doc["apiServerIP"];
    String apiServerEndpoint = doc["apiServerEndpoint"];
    String apiServerToken = doc["apiServerToken"];
    String deviceName = doc["deviceName"];
    String deviceUser = doc["deviceUser"];
    String devicePassword = doc["devicePassword"];
    String authToken = doc["authToken"];

    // Ensure no buffer overflows
    wifiSSID.toCharArray(config.wifiSSID, sizeof(config.wifiSSID));
    wifiPassword.toCharArray(config.wifiPassword, sizeof(config.wifiPassword));
    // wifiMAC.toCharArray(config.wifiMAC, sizeof(config.wifiMAC));
    parseMAC(wifiMAC, config.wifiMAC);
    apiServerIP.toCharArray(config.apiServerIP, sizeof(config.apiServerIP));
    apiServerEndpoint.toCharArray(config.apiServerEndpoint, sizeof(config.apiServerEndpoint));
    apiServerToken.toCharArray(config.apiServerToken, sizeof(config.apiServerToken));
    deviceName.toCharArray(config.deviceName, sizeof(config.deviceName));
    deviceUser.toCharArray(config.deviceUser, sizeof(config.deviceUser));
    devicePassword.toCharArray(config.devicePassword, sizeof(config.devicePassword));
    authToken.toCharArray(config.authToken, sizeof(config.authToken));

    // Save the updated configuration to EEPROM
    saveConfig();

    // Send response back to the client
    server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Configuration saved! Restarting the device.\"}");
  } else {
    server.send(405, "application/json", "{\"status\":\"error\",\"message\":\"Method not allowed!\"}");
  }
}

// Device Reboot handler
void handleReboot(){
  String token = server.arg("token");
  if(!token.equals(config.authToken)) {
    server.send(401, "application/json", "{\"status\":\"error\",\"message\":\"Access denied.\"}");
    return;
  }
  ESP.restart();
}

// Device Settings to Web form
void handleLoadSettings(){
  if (server.hasHeader("Authorization")) {
    String token = server.header("Authorization");
    if(token.equals(config.authToken)) {
      server.send(200, "application/json", getConfigAsJson());   
    } else {
      server.send(401, "application/json", "{\"status\":\"error\",\"message\":\"Access denied.\"}");
    }
  } else {
    server.send(401, "application/json", "{\"status\":\"error\",\"message\":\"Access denied.\"}");
  }
}

void handleGenerateToken() {
  String token = generateToken(16);
  server.send(200, "application/json", "{\"status\":\"success\",\"authToken\":\""+token+"\"}");
}

// API Server status update 
void sendServer(bool state){
  http.begin(wifi, "http://"+String(config.apiServerIP)+"/iapi/setstate");
  String post = "token="+String(config.apiServerToken)+"&state="+(state?"on":"off"); // The API server will determine the device by token (ToDO)
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  // int httpCode = http.POST(post);
  http.end();  
}

// Server load handlers
void handleOn(){
  if (server.hasHeader("Authorization")) {
    String token = server.header("Authorization");
    if(token.equals(config.authToken)) {
      turnLoadOn();
      server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Load On.\"}");
    }
  }
  server.send(401, "application/json", "{\"status\":\"error\",\"message\":\"Access denied.\"}");
  return;
}

void handleOff(){
  if (server.hasHeader("Authorization")) {
    String token = server.header("Authorization");
    if(token.equals(config.authToken)) {
      turnLoadOff();
      server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Load Off.\"}");
    }
  }
  server.send(401, "application/json", "{\"status\":\"error\",\"message\":\"Access denied.\"}");
  return;
}


// Manipulations with the load
// Switch On
void turnLoadOn(){
  digitalWrite(load, HIGH);
  load_on = true;
}

// Switch Off
void turnLoadOff(){
  digitalWrite(load, LOW);
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

void setup(void){
  Serial.begin(9600);
  pinMode(load, OUTPUT);
  pinMode(button, INPUT_PULLUP); // Important to make INPUT_PULLUP
  turnLoadOff();
  
  if (loadConfig()) {
    WiFi.mode(WIFI_STA);

    if (validateMAC(config.wifiMAC) == true) {
      uint8_t wifiMAC[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
      memcpy(wifiMAC, config.wifiMAC, sizeof(config.wifiMAC));
      if (wifi_set_macaddr(STATION_IF, wifiMAC)) {
        Serial.println("MAC address successfully set.");
      } else {
        Serial.println("Failed to set MAC address.");
      }
    }
    WiFi.hostname(config.deviceName);
    WiFi.begin(config.wifiSSID, config.wifiPassword);

    // Waiting for WiFi connection
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      attempts++;
    }
  } else {
    Serial.println("No Config loaded.");
  }
  
  if(WiFi.status() != WL_CONNECTED)  {
    WiFi.mode(WIFI_AP); // Set to Access Point mode
    WiFi.softAP(apSSID, apPassword);
    Serial.println("AP Mode");
  }
  
  // Configure routes
  server.on("/", handleRoot);
  server.on("/on",  HTTP_POST, handleOn);
  server.on("/off", HTTP_POST, handleOff);
  server.on("/login", HTTP_GET, handleLoginForm);
  server.on("/login", HTTP_POST, handleLogin);
  server.on("/restart",  HTTP_POST, handleReboot);
  server.on("/settings", HTTP_GET, handleSettingsForm);
  server.on("/settings", HTTP_POST, handleSettingsSave);
  server.on("/settings/load", HTTP_GET, handleLoadSettings);
  server.on("/settings/token", HTTP_GET, handleGenerateToken);
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
  Serial.println(WiFi.macAddress());
}

void loop(void){
  static unsigned long lastToggleTime = 0;
  const unsigned long debounceDelay = 300; 
  
  ArduinoOTA.handle();
  server.handleClient();

  button_state = digitalRead(button);
  if (button_state == HIGH && can_toggle) {
    unsigned long currentTime = millis();
    if (currentTime - lastToggleTime > debounceDelay) {
      toggleLoadState();    
      can_toggle = false;
      lastToggleTime = currentTime;
    }
  } else if(button_state == LOW){
    can_toggle = true;
  }
}