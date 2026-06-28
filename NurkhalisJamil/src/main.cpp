#include <WiFi.h>
#include <WebServer.h>

#include "telegram_config.h"
#include "telegram_notify.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

// ======================================
// WIFI
// ======================================

const char* ssid = "NAMA_WIFI";
const char* password = "PASSWORD_WIFI";

WebServer server(80);

// ======================================
// OLED
// ======================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ======================================
// DHT
// ======================================

#define DHTPIN 4
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);

// ======================================
// PIN
// ======================================

#define POT_PIN 34
#define RELAY_PIN 18
#define BUZZER_PIN 19

// ======================================
// VARIABLE
// ======================================

float voltage;
float temperature;

const int MAX_LOG_LINES = 8;
String logLines[MAX_LOG_LINES];
int logCount = 0;
String currentStatus = "NORMAL";

// ======================================
// FUNCTION PROTOTYPE
// ======================================

void startupScreen();
void addLog(String message);
void displayNormal();
void displayWarning();
void displayCritical();
void handleRoot();

// ======================================
// LOG FUNCTION
// ======================================

void addLog(String message) {
  if (logCount < MAX_LOG_LINES) {
    logLines[logCount++] = message;
  } else {
    for (int i = 0; i < MAX_LOG_LINES - 1; i++) {
      logLines[i] = logLines[i + 1];
    }
    logLines[MAX_LOG_LINES - 1] = message;
  }
}

// ======================================
// SETUP
// ======================================

void setup() {

  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(RELAY_PIN, HIGH);
  digitalWrite(BUZZER_PIN, LOW);

  dht.begin();

  // ======================================
  // OLED INIT
  // ======================================

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED gagal");
    while (1);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  // ======================================
  // WIFI CONNECT
  // ======================================

  WiFi.mode(WIFI_AP_STA);

  const char* wifiSsid = ssid;
  const char* wifiPassword = password;

  if (String(ssid) == "NAMA_WIFI" || String(password) == "PASSWORD_WIFI") {
    Serial.println("Placeholder WiFi credentials detected, using Wokwi-GUEST");
    wifiSsid = "Wokwi-GUEST";
    wifiPassword = "";
  }

  WiFi.begin(wifiSsid, wifiPassword);

  Serial.print("Connecting WiFi");

  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 20000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connection failed");
    Serial.println("Check SSID/password or network availability");

    Serial.println("Starting fallback access point VoltGuard-AP...");
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    WiFi.softAP("VoltGuard-AP");
    delay(500);
    Serial.print("AP IP Address: ");
    Serial.println(WiFi.softAPIP());
  }
  else {
    Serial.println("");
    Serial.println("WiFi Connected");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  }

  // ======================================
  // WEB SERVER
  // ======================================

  server.on("/", handleRoot);

  server.begin();

  Serial.println("Web Server Started");
  Serial.print("Access it at http://");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(WiFi.localIP());
  } else {
    Serial.print(WiFi.softAPIP());
  }
  Serial.println(":80");
  Serial.println("In Wokwi, this is available via http://localhost:8180");

  addLog("System booted");
  startupScreen();
}

// ======================================
// LOOP
// ======================================

void loop() {
  // ======================================
  // READ TEMPERATURE
  // ======================================

  temperature = dht.readTemperature();

  // ======================================
  // READ POTENTIOMETER
  // ======================================

  int potValue = analogRead(POT_PIN);

  voltage = map(potValue, 0, 4095, 180, 260);

  // ======================================
  // SERIAL MONITOR
  // ======================================

  Serial.print("Voltage: ");
  Serial.print(voltage);
  Serial.print(" V | Temp: ");
  Serial.print(temperature);
  Serial.println(" C");

  String statusNow = "NORMAL";

  // ======================================
  // NORMAL STATE
  // ======================================

  if (voltage < 230 && temperature < 45) {

    digitalWrite(RELAY_PIN, HIGH);

    noTone(BUZZER_PIN);

    displayNormal();
  }

  // ======================================
  // WARNING STATE
  // ======================================

  else if ((voltage >= 230 && voltage < 240) ||
           (temperature >= 45 && temperature < 60)) {

    digitalWrite(RELAY_PIN, HIGH);

    tone(BUZZER_PIN, 1000);
    delay(200);
    noTone(BUZZER_PIN);

    displayWarning();
    statusNow = "WARNING";
  }

  // ======================================
  // CRITICAL STATE
  // ======================================

  else {

    digitalWrite(RELAY_PIN, LOW);

    tone(BUZZER_PIN, 2000);

    displayCritical();
    statusNow = "CRITICAL";
  }

  if (statusNow != currentStatus) {
    currentStatus = statusNow;
    addLog("Status: " + currentStatus + " | V=" + String(voltage, 1) + " | T=" + String(temperature, 1));

    // Kirim notifikasi ke Telegram sesuai konfigurasi (pesan dalam Bahasa Indonesia)
    #if TELEGRAM_ENABLED
    if (currentStatus == "WARNING" && TELEGRAM_NOTIFY_WARNING) {
      sendTelegramNotification("Peringatan: Sistem pada status WARNING\nV=" + String(voltage, 1) + " V | T=" + String(temperature, 1) + " C");
    } else if (currentStatus == "CRITICAL" && TELEGRAM_NOTIFY_CRITICAL) {
      sendTelegramNotification("KRITIS: Kondisi bahaya! Power dip/putus.\nV=" + String(voltage, 1) + " V | T=" + String(temperature, 1) + " C");
    }
    #endif
  }

  // Tangani permintaan web setelah memperbarui variabel sensor
  server.handleClient();

  delay(1000);
}

// ======================================
// STARTUP SCREEN
// ======================================

void startupScreen() {

  display.clearDisplay();

  display.setTextSize(2);
  display.setCursor(10, 15);
  display.println("VoltGuard");

  display.setTextSize(1);
  display.setCursor(25, 45);
  display.println("System Booting");

  display.display();

  delay(2000);
}

// ======================================
// NORMAL DISPLAY
// ======================================

void displayNormal() {

  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("SYSTEM NORMAL");

  display.setCursor(0, 20);
  display.print("Voltage : ");
  display.print(voltage);
  display.println(" V");

  display.setCursor(0, 35);
  display.print("Temp    : ");
  display.print(temperature);
  display.println(" C");

  display.setCursor(0, 50);
  display.println("Relay : ON");

  display.display();
}

// ======================================
// WARNING DISPLAY
// ======================================

void displayWarning() {

  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(20, 0);
  display.println("WARNING!");

  display.setCursor(0, 20);
  display.print("Voltage : ");
  display.print(voltage);
  display.println(" V");

  display.setCursor(0, 35);
  display.print("Temp    : ");
  display.print(temperature);
  display.println(" C");

  display.setCursor(0, 50);
  display.println("Check System!");

  display.display();
}

// ======================================
// CRITICAL DISPLAY
// ======================================

void displayCritical() {

  display.clearDisplay();

  display.setTextSize(2);
  display.setCursor(10, 0);
  display.println("DANGER");

  display.setTextSize(1);

  display.setCursor(0, 28);
  display.print("Voltage : ");
  display.print(voltage);
  display.println(" V");

  display.setCursor(0, 42);
  display.print("Temp    : ");
  display.print(temperature);
  display.println(" C");

  display.setCursor(0, 56);
  display.println("POWER CUT OFF");

  display.display();
}

// ======================================
// WEB SERVER PAGE
// ======================================

void handleRoot() {

  String statusSystem;

  if (voltage < 230 && temperature < 45) {
    statusSystem = "NORMAL";
  }
  else if ((voltage >= 230 && voltage < 240) ||
           (temperature >= 45 && temperature < 60)) {
    statusSystem = "WARNING";
  }
  else {
    statusSystem = "CRITICAL";
  }

  String statusClass = "normal";
  if (statusSystem == "WARNING") {
    statusClass = "warning";
  }
  else if (statusSystem == "CRITICAL") {
    statusClass = "critical";
  }

  String html = R"rawliteral(

<!DOCTYPE html>
<html>

<head>

<meta charset="UTF-8">
<meta http-equiv="refresh" content="2">

<title>VoltGuard ESP32</title>

<style>
  :root {
    --bg: #0f172a;
    --card: #111827;
    --text: #f8fafc;
    --muted: #94a3b8;
    --normal: #22c55e;
    --warning: #f59e0b;
    --critical: #ef4444;
    --accent: #38bdf8;
  }

  * { box-sizing: border-box; }

  body {
    margin: 0;
    background: linear-gradient(135deg, var(--bg), #1e293b);
    color: var(--text);
    font-family: Arial, sans-serif;
    text-align: center;
    padding: 24px 16px;
  }

  .container {
    max-width: 420px;
    margin: 0 auto;
  }

  h1 {
    margin: 0 0 8px;
    font-size: 28px;
    color: var(--accent);
  }

  .subtitle {
    margin: 0 0 20px;
    color: var(--muted);
    font-size: 14px;
  }

  .card {
    background: rgba(17, 24, 39, 0.95);
    border: 1px solid rgba(255,255,255,0.08);
    box-shadow: 0 8px 24px rgba(0,0,0,0.25);
    padding: 18px;
    border-radius: 16px;
    margin-bottom: 14px;
  }

  .card h2 {
    margin: 0 0 10px;
    font-size: 16px;
    color: var(--muted);
    text-transform: uppercase;
    letter-spacing: 1px;
  }

  .value {
    font-size: 32px;
    font-weight: bold;
    margin-top: 6px;
  }

  .status-normal { color: var(--normal); }
  .status-warning { color: var(--warning); }
  .status-critical { color: var(--critical); }

  .badge {
    display: inline-block;
    padding: 6px 12px;
    border-radius: 999px;
    font-weight: bold;
    margin-top: 4px;
    background: rgba(56, 189, 248, 0.16);
    color: var(--accent);
  }

  .log-list {
    text-align: left;
    margin-top: 6px;
  }

  .log-item {
    padding: 8px 10px;
    margin-bottom: 8px;
    border-radius: 10px;
    background: rgba(255,255,255,0.06);
    font-size: 14px;
    color: var(--text);
  }
</style>

</head>

<body>
  <div class="container">
    <h1>VoltGuard ESP32</h1>
    <p class="subtitle">Monitoring sistem listrik dan suhu</p>

    <div class="card">
      <h2>Status Sistem</h2>
      <div class="value status-)rawliteral";

  html += statusClass;

  html += R"rawliteral(
">
)rawliteral";

  html += statusSystem;

  html += R"rawliteral(
</div>
      <div class="badge">Realtime Monitoring</div>
    </div>

    <div class="card">
      <h2>Voltage</h2>
      <div class="value">
)rawliteral";

  html += String(voltage);

  html += R"rawliteral(
 V
</div>
    </div>

    <div class="card">
      <h2>Temperature</h2>
      <div class="value">
)rawliteral";

  html += String(temperature);

  html += R"rawliteral(
 °C
</div>
    </div>

    <div class="card">
      <h2>Log Aktivitas</h2>
      <div class="log-list">
)rawliteral";

  if (logCount == 0) {
    html += "<div class=\"log-item\">Belum ada log</div>";
  } else {
    for (int i = logCount - 1; i >= 0; i--) {
      html += "<div class=\"log-item\">" + logLines[i] + "</div>";
    }
  }

  html += R"rawliteral(
      </div>
    </div>
  </div>
</body>
</html>

)rawliteral";

  server.send(200, "text/html", html);
}
