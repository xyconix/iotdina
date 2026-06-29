#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <Stepper.h>
#include "telegram_notify.h"
#include <telegram_config.h>

// ================= WIFI =================
const char* ssid = "NAMA_WIFI";
const char* password = "PASSWORD_WIFI";
WebServer server(80);

// ================= OLED =================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ================= PIN =================
#define SOIL_PIN 34
#define DHT_PIN 4

// ================= DHT =================
#define DHTTYPE DHT22
DHT dht(DHT_PIN, DHTTYPE);

// ================= STEPPER =================
// Stepper motor sebagai simulasi pompa air

const int stepsPerRevolution = 200;

// IN1, IN2, IN3, IN4
Stepper pumpMotor(stepsPerRevolution, 14, 27, 26, 25);

// ================= VARIABLE =================
int soilValue = 0;
int soilPercent = 0;

float temperature = 0;
float humidity = 0;

// Threshold tanah kering
int dryThreshold = 40;

// Status pompa
bool pumpStatus = false;
bool lastPumpStatus = false;

// ================= LOG =================
const int MAX_LOG_LINES = 16;
String logLines[MAX_LOG_LINES];
int logCount = 0;

void addLog(const String &message) {
  if (logCount < MAX_LOG_LINES) {
    logLines[logCount++] = message;
  } else {
    for (int i = 0; i < MAX_LOG_LINES - 1; i++) {
      logLines[i] = logLines[i + 1];
    }
    logLines[MAX_LOG_LINES - 1] = message;
  }
}

String formatSensorLine(const String &label, const String &value) {
  return "<tr><td>" + label + "</td><td>" + value + "</td></tr>";
}

void setupWebServer();
void handleRoot();
void handleData();
void handleLogs();
void sendPumpNotification();

void setup() {

  Serial.begin(115200);

  // ================= DHT =================
  dht.begin();

  // ================= STEPPER =================
  pumpMotor.setSpeed(60);

  // ================= OLED =================
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED gagal");
    while (true);
  }

  display.clearDisplay();
  display.setTextColor(WHITE);

  // ================= SPLASH SCREEN =================
  display.setTextSize(2);
  display.setCursor(10, 10);
  display.println("SMART");
  display.println("PLANT");
  display.display();

  delay(2000);

  // ================= WIFI CONNECT =================
  WiFi.mode(WIFI_AP_STA);
  const char* wifiSsid = ssid;
  const char* wifiPassword = password;

  if (String(ssid) == "NAMA_WIFI" || String(password) == "PASSWORD_WIFI") {
    wifiSsid = "Wokwi-GUEST";
    wifiPassword = "";
  }

  WiFi.begin(wifiSsid, wifiPassword);

  Serial.print("Connecting to WiFi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.print("Connected: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("");
    Serial.println("WiFi failed, starting AP mode...");
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    WiFi.softAP("SmartPlant-AP");
    delay(500);
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
  }

  setupWebServer();
  addLog("Web dashboard started");

  if (WiFi.status() == WL_CONNECTED) {
    sendTelegramNotification("Smart Plant started.\nMonitoring soil, temperature, and humidity.");
    addLog("Telegram: startup notification sent");
  }
}

void handleRoot() {
  String html = "<!DOCTYPE html><html lang=\"en\"><head>";
  html += "<meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
  html += "<title>Smart Plant Dashboard</title>";
  html += "<style>body{font-family:Arial,sans-serif;background:#111;color:#eee;margin:0;padding:0;}";
  html += "header{padding:20px;text-align:center;background:#1a73e8;}";
  html += "section{padding:20px;max-width:900px;margin:auto;}";
  html += "h1,h2{margin:0 0 16px;}";
  html += ".card{background:#181818;border:1px solid #2a2a2a;border-radius:12px;padding:18px;margin-bottom:18px;box-shadow:0 10px 30px rgba(0,0,0,0.25);}";
  html += ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:16px;}";
  html += "table{width:100%;border-collapse:collapse;}td{padding:10px;border-bottom:1px solid #2a2a2a;}";
  html += "a.button{display:inline-block;padding:10px 16px;color:#fff;background:#1a73e8;border-radius:8px;text-decoration:none;}";
  html += "</style></head><body>";
  html += "<header><h1>Smart Plant Dashboard</h1><p>Monitoring kelembaban, suhu, dan kondisi pompa.</p></header>";
  html += "<section>";
  html += "<div class=\"grid\">";
  html += "<div class=\"card\"><h2>Soil</h2><table>";
  html += formatSensorLine("Soil Percent", String(soilPercent) + "%");
  html += formatSensorLine("Pump Status", pumpStatus ? "ON" : "OFF");
  html += "</table></div>";
  html += "<div class=\"card\"><h2>Environment</h2><table>";
  html += formatSensorLine("Temperature", String(temperature, 1) + " °C");
  html += formatSensorLine("Humidity", String(humidity, 1) + " %");
  html += "</table></div>";
  html += "</div>";
  html += "<div class=\"card\"><h2>Realtime Data</h2>";
  html += "<p>Klik tombol di bawah untuk melihat JSON dan log.</p>";
  html += "<a class=\"button\" href=\"/data\">Lihat JSON</a> ";
  html += "<a class=\"button\" href=\"/logs\">Lihat Log</a>";
  html += "</div>";
  html += "</section></body></html>";

  server.send(200, "text/html", html);
}

void handleData() {
  String json = "{";
  json += "\"soil_percent\": " + String(soilPercent) + ",";
  json += "\"temperature\": " + String(temperature, 1) + ",";
  json += "\"humidity\": " + String(humidity, 1) + ",";
  json += "\"pump_status\": \"" + String(pumpStatus ? "ON" : "OFF") + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleLogs() {
  String html = "<!DOCTYPE html><html lang=\"en\"><head>";
  html += "<meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
  html += "<title>Smart Plant Logs</title>";
  html += "<style>body{font-family:Arial,sans-serif;background:#111;color:#eee;margin:0;padding:0;}";
  html += "header{padding:20px;text-align:center;background:#1a73e8;}";
  html += "section{padding:20px;max-width:900px;margin:auto;}";
  html += ".card{background:#181818;border:1px solid #2a2a2a;border-radius:12px;padding:18px;margin-bottom:18px;box-shadow:0 10px 30px rgba(0,0,0,0.25);}";
  html += "ul{list-style:none;padding:0;margin:0;}li{padding:10px;border-bottom:1px solid #2a2a2a;}";
  html += "a.button{display:inline-block;padding:10px 16px;color:#fff;background:#1a73e8;border-radius:8px;text-decoration:none;}";
  html += "</style></head><body>";
  html += "<header><h1>Smart Plant Logs</h1><p>Riwayat data terakhir dari sistem.</p></header>";
  html += "<section><div class=\"card\"><h2>Recent logs</h2><ul>";

  for (int i = 0; i < logCount; i++) {
    html += "<li>" + logLines[i] + "</li>";
  }

  if (logCount == 0) {
    html += "<li>No log available.</li>";
  }

  html += "</ul><p><a class=\"button\" href=\"/\">Back to dashboard</a></p></div></section></body></html>";
  server.send(200, "text/html", html);
}

void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/logs", handleLogs);
  server.begin();
}

void loop() {

  // ================= BACA SOIL =================
  soilValue = analogRead(SOIL_PIN);

  // Konversi ke persen
  soilPercent = map(soilValue, 0, 4095, 0, 100);

  // Batasi
  soilPercent = constrain(soilPercent, 0, 100);

  // ================= BACA DHT =================
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  // Cegah NaN
  if (isnan(temperature) || isnan(humidity)) {

    Serial.println("DHT Error!");

    temperature = 0;
    humidity = 0;
  }

  // ================= LOGIKA POMPA =================
  if (soilPercent < dryThreshold) {

    // Motor berputar
    pumpMotor.step(50);

    pumpStatus = true;

  } else {

    // Motor berhenti
    pumpStatus = false;
  }

  sendPumpNotification();

  // ================= SERIAL MONITOR =================
  Serial.println("===== SMART PLANT =====");

  Serial.print("Soil Value   : ");
  Serial.println(soilValue);

  Serial.print("Soil Percent : ");
  Serial.print(soilPercent);
  Serial.println("%");

  Serial.print("Temperature  : ");
  Serial.print(temperature);
  Serial.println(" C");

  Serial.print("Humidity     : ");
  Serial.print(humidity);
  Serial.println("%");

  Serial.print("Pump Status  : ");

  if (pumpStatus) {
    Serial.println("ON");
  } else {
    Serial.println("OFF");
  }

  Serial.println("========================");
  Serial.println();

  // ================= OLED DISPLAY =================
  display.clearDisplay();

  // Judul
  display.setTextSize(1);
  display.setCursor(10, 0);
  display.println("SMART PLANT MONITOR");

  // Soil
  display.setCursor(0, 16);
  display.print("Soil : ");
  display.print(soilPercent);
  display.println("%");

  // Temperature
  display.setCursor(0, 30);
  display.print("Temp : ");
  display.print(temperature);
  display.println(" C");

  // Humidity
  display.setCursor(0, 44);
  display.print("Hum  : ");
  display.print(humidity);
  display.println("%");

  // Pump Status
  display.setCursor(0, 56);

  if (pumpStatus) {
    display.print("Pump : ON");
  } else {
    display.print("Pump : OFF");
  }

  display.display();

  // ================= WEB SERVER =================
  server.handleClient();

  // ================= LOG UPDATE =================
  String logEntry = "Soil=" + String(soilPercent) + "% | Temp=" + String(temperature, 1) + "C | Hum=" + String(humidity, 1) + "% | Pump=" + String(pumpStatus ? "ON" : "OFF");
  addLog(logEntry);

  delay(1000);
}

void sendPumpNotification() {
  if (pumpStatus && !lastPumpStatus) {
    sendTelegramNotification("Smart Plant: tanah kering terdeteksi.\nPompa menyala.");
    addLog("Telegram: Pump ON notification sent");
  } else if (!pumpStatus && lastPumpStatus) {
    sendTelegramNotification("Smart Plant: kelembaban tanah normal.\nPompa dimatikan.");
    addLog("Telegram: Pump OFF notification sent");
  }

  lastPumpStatus = pumpStatus;
}