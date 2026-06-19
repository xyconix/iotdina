#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <WebServer.h>

// =======================
// PIN CONFIG
// =======================

#define POT_PIN     34
#define LED_YELLOW  25
#define LED_GREEN   26
#define LED_RED     27
#define BUZZER_PIN  19
#define SERVO_PIN   18
#define TRIG_PIN    5
#define ECHO_PIN    17
#define DHT_PIN     4
#define DHT_TYPE    DHT22
#define BUTTON_PIN  16

// =======================
// WiFi CONFIG
// =======================

// Wokwi guest network uses this exact SSID casing.
const char* sta_ssid = "Wokwi-GUEST";
const char* sta_password = "";
const char* ap_ssid = "ESP32-Driver";
const char* ap_password = "12345678";

// =======================
// OBJECTS
// =======================

LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_MPU6050 mpu;
Servo alarmServo;
WebServer server(80);

// =======================
// GLOBAL VARIABLES
// =======================

int driverScore = 100;
bool alarmMuted = false;
unsigned long lastPageChange = 0;
int currentPage = 0;
unsigned long sleepStartTime = 0;
bool sleeping = false;
bool wifiConnected = false;

String driverStatus = "FOCUS";
String overallStatus = "AMAN";
float cabinTemp = 0;
float cabinHum = 0;
float distanceCM = 0;
bool unstableVehicle = false;

// =======================
// HC-SR04
// =======================

float readDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(5);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  
  if (duration == 0)
    return 999;
  
  return duration * 0.034 / 2.0;
}

// =======================
// BUZZER
// =======================

void beepWarning() {
  tone(BUZZER_PIN, 1500, 200);
}

void beepDanger() {
  tone(BUZZER_PIN, 2500);
}

void stopBuzzer() {
  noTone(BUZZER_PIN);
}

// =======================
// SERVO SHAKE
// =======================

void shakeServo() {
  alarmServo.write(45);
  delay(100);
  alarmServo.write(135);
  delay(100);
  alarmServo.write(90);
}

// =======================
// LCD PAGES
// =======================

void showPage() {
  lcd.clear();
  
  switch (currentPage) {
    case 0:
      lcd.setCursor(0,0);
      lcd.print("Score:");
      lcd.print(driverScore);
      lcd.setCursor(0,1);
      lcd.print(overallStatus);
      break;
      
    case 1:
      lcd.setCursor(0,0);
      lcd.print("Temp:");
      lcd.print(cabinTemp,1);
      lcd.print("C");
      lcd.setCursor(0,1);
      lcd.print("Hum:");
      lcd.print(cabinHum,0);
      lcd.print("%");
      break;
      
    case 2:
      lcd.setCursor(0,0);
      lcd.print("Dist:");
      lcd.print(distanceCM,0);
      lcd.print("cm");
      lcd.setCursor(0,1);
      if(distanceCM < 50)
        lcd.print("TOO CLOSE");
      else
        lcd.print("SAFE");
      break;
      
    case 3:
      lcd.setCursor(0,0);
      lcd.print("Driver:");
      lcd.setCursor(0,1);
      lcd.print(driverStatus);
      break;
  }
}

// =======================
// WEB SERVER HANDLER
// =======================

String htmlPage() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta http-equiv="refresh" content="2">
  <title>Driver Guardian</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: 'Segoe UI', Arial, sans-serif;
      background: #0f172a;
      color: white;
      min-height: 100vh;
      padding: 20px;
      display: flex;
      justify-content: center;
      align-items: center;
    }
    .container {
      max-width: 800px;
      width: 100%;
    }
    .header {
      text-align: center;
      margin-bottom: 30px;
    }
    .header h1 {
      font-size: 32px;
      font-weight: 700;
      background: linear-gradient(135deg, #60a5fa, #a78bfa);
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
    }
    .header p {
      color: #94a3b8;
      margin-top: 5px;
    }
    .header .badge {
      display: inline-block;
      background: rgba(34, 197, 94, 0.2);
      color: #22c55e;
      padding: 4px 12px;
      border-radius: 20px;
      font-size: 12px;
      margin-top: 8px;
    }
    .card {
      background: rgba(255,255,255,0.05);
      backdrop-filter: blur(10px);
      border: 1px solid rgba(255,255,255,0.1);
      padding: 20px;
      margin: 10px 0;
      border-radius: 16px;
      box-shadow: 0 8px 32px rgba(0,0,0,0.3);
      transition: transform 0.2s;
    }
    .card:hover {
      transform: translateY(-2px);
    }
    .status-main {
      text-align: center;
      padding: 25px;
    }
    .status-main .label {
      font-size: 14px;
      color: #94a3b8;
      text-transform: uppercase;
      letter-spacing: 2px;
    }
    .status-main .value {
      font-size: 42px;
      font-weight: 700;
      margin-top: 10px;
    }
    .status-main .value .icon {
      display: block;
      font-size: 48px;
      margin-bottom: 5px;
    }
    .status-ok { color: #22c55e; }
    .status-warn { color: #facc15; }
    .status-danger { color: #ef4444; }
    
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
      gap: 12px;
      margin-top: 10px;
    }
    .grid .card {
      text-align: center;
      margin: 0;
      padding: 15px;
    }
    .grid .card .label {
      font-size: 12px;
      color: #94a3b8;
      text-transform: uppercase;
      letter-spacing: 1px;
    }
    .grid .card .value {
      font-size: 24px;
      font-weight: 600;
      margin-top: 5px;
    }
    .grid .card .value small {
      font-size: 14px;
      font-weight: 400;
      color: #94a3b8;
    }
    .led-indicator {
      display: flex;
      justify-content: center;
      gap: 15px;
      padding: 10px;
    }
    .led {
      width: 20px;
      height: 20px;
      border-radius: 50%;
      border: 2px solid rgba(255,255,255,0.1);
      transition: all 0.3s;
    }
    .led-green { background: #22c55e; box-shadow: 0 0 20px rgba(34,197,94,0.5); }
    .led-yellow { background: #facc15; box-shadow: 0 0 20px rgba(250,204,21,0.5); }
    .led-red { background: #ef4444; box-shadow: 0 0 20px rgba(239,68,68,0.5); }
    .led-off { background: #334155; }
    
    .footer {
      text-align: center;
      margin-top: 20px;
      color: #475569;
      font-size: 12px;
    }
    .footer a {
      color: #60a5fa;
      text-decoration: none;
    }
    
    @media (max-width: 480px) {
      .header h1 { font-size: 24px; }
      .status-main .value { font-size: 32px; }
      .grid .card .value { font-size: 20px; }
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <h1>🚗 DRIVER GUARDIAN</h1>
      <p>Real-time Monitoring System</p>
      <span class="badge">🔴 LIVE</span>
    </div>
    
    <div class="card status-main">
      <div class="label">Overall Status</div>
      <div class="value )rawliteral";
  
  // Add status class and icon
  if (overallStatus == "AMAN") {
    html += "status-ok\"><span class=\"icon\">✅</span>";
  } else if (overallStatus == "WASPADA") {
    html += "status-warn\"><span class=\"icon\">⚠️</span>";
  } else {
    html += "status-danger\"><span class=\"icon\">🚨</span>";
  }
  
  html += overallStatus;
  html += R"rawliteral(</div>
    </div>
    
    <div class="grid">
      <div class="card">
        <div class="label">Driver</div>
        <div class="value )rawliteral";
  
  if (driverStatus == "FOCUS") {
    html += "status-ok";
  } else if (driverStatus == "DROWSY") {
    html += "status-warn";
  } else {
    html += "status-danger";
  }
  
  html += "\">";
  html += driverStatus;
  html += R"rawliteral(</div>
      </div>
      
      <div class="card">
        <div class="label">Score</div>
        <div class="value )rawliteral";
  
  if (driverScore >= 70) {
    html += "status-ok";
  } else if (driverScore >= 40) {
    html += "status-warn";
  } else {
    html += "status-danger";
  }
  
  html += "\">";
  html += String(driverScore);
  html += R"rawliteral(<small>/100</small></div>
      </div>
      
      <div class="card">
        <div class="label">Temperature</div>
        <div class="value ");
  
  if (cabinTemp > 30) {
    html += "status-warn";
  }
  if (cabinTemp > 35) {
    html += "status-danger";
  }
  
  html += "\">";
  html += String(cabinTemp, 1);
  html += R"rawliteral(<small> °C</small></div>
      </div>
      
      <div class="card">
        <div class="label">Humidity</div>
        <div class="value">)rawliteral";
  html += String(cabinHum, 0);
  html += R"rawliteral(<small> %</small></div>
      </div>
      
      <div class="card">
        <div class="label">Distance</div>
        <div class="value ");

  if (distanceCM < 80) {
    html += "status-warn";
  }
  if (distanceCM < 50) {
    html += "status-danger";
  }
  
  html += "\">";
  html += String(distanceCM, 0);
  html += R"rawliteral(<small> cm</small></div>
      </div>
    </div>
    
    <div class="card">
      <div class="label" style="text-align:center; margin-bottom:10px;">Live Indicator</div>
      <div class="led-indicator">
        <div class="led )rawliteral";
  
  if (overallStatus == "AMAN") {
    html += "led-green";
  } else {
    html += "led-off";
  }
  
  html += R"rawliteral("></div>
        <div class="led )rawliteral";
  
  if (overallStatus == "WASPADA") {
    html += "led-yellow";
  } else {
    html += "led-off";
  }
  
  html += R"rawliteral("></div>
        <div class="led )rawliteral";
  
  if (overallStatus == "BAHAYA") {
    html += "led-red";
  } else {
    html += "led-off";
  }
  
  html += R"rawliteral("></div>
      </div>
      <div style="display:flex; justify-content:center; gap:30px; font-size:12px; color:#94a3b8;">
        <span>🟢 Aman</span>
        <span>🟡 Waspada</span>
        <span>🔴 Bahaya</span>
      </div>
    </div>
    
    <div class="footer">
      System v2.0 | ESP32 Driver Guardian<br>
      <span id="timestamp"></span>
    </div>
  </div>
  
  <script>
    document.getElementById('timestamp').textContent = 'Last update: ' + new Date().toLocaleTimeString();
  </script>
</body>
</html>
)rawliteral";

  return html;
}

String htmlPageModern() {
  String overallClass = "status-ok";
  String overallHint = "System is in a safe state.";
  if (overallStatus == "WASPADA") {
    overallClass = "status-warn";
    overallHint = "Monitor driver attention and distance.";
  } else if (overallStatus == "BAHAYA") {
    overallClass = "status-danger";
    overallHint = "Immediate action recommended.";
  }

  String driverClass = "status-ok";
  if (driverStatus == "DROWSY") {
    driverClass = "status-warn";
  } else if (driverStatus == "SLEEPING") {
    driverClass = "status-danger";
  }

  String scoreClass = "status-ok";
  if (driverScore < 70) scoreClass = "status-warn";
  if (driverScore < 40) scoreClass = "status-danger";

  String tempClass = "status-ok";
  if (cabinTemp > 35) {
    tempClass = "status-danger";
  } else if (cabinTemp > 30) {
    tempClass = "status-warn";
  }

  String distanceClass = "status-ok";
  if (distanceCM < 50) {
    distanceClass = "status-danger";
  } else if (distanceCM < 80) {
    distanceClass = "status-warn";
  }

  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta http-equiv="refresh" content="2">
  <title>Driver Guardian</title>
  <style>
    :root {
      --bg: #050816;
      --panel: rgba(11, 18, 34, 0.78);
      --stroke: rgba(255, 255, 255, 0.10);
      --text: #e8f0ff;
      --muted: #93a7c4;
      --cyan: #63d7ff;
      --green: #39d98a;
      --yellow: #f5c451;
      --red: #ff6b6b;
      --shadow: 0 30px 80px rgba(0, 0, 0, 0.48);
    }
    * { box-sizing: border-box; }
    html, body { min-height: 100%; }
    body {
      margin: 0;
      font-family: "Trebuchet MS", "Segoe UI", system-ui, sans-serif;
      color: var(--text);
      background:
        radial-gradient(circle at 10% 10%, rgba(99, 215, 255, 0.18), transparent 28%),
        radial-gradient(circle at 90% 15%, rgba(57, 217, 138, 0.14), transparent 26%),
        radial-gradient(circle at 85% 90%, rgba(255, 107, 107, 0.14), transparent 28%),
        linear-gradient(135deg, #030712 0%, #08111f 45%, #040816 100%);
      overflow-x: hidden;
    }
    body::before, body::after {
      content: "";
      position: fixed;
      border-radius: 50%;
      filter: blur(50px);
      pointer-events: none;
      opacity: 0.55;
      animation: float 12s ease-in-out infinite;
    }
    body::before {
      width: 240px;
      height: 240px;
      left: -70px;
      top: -60px;
      background: rgba(99, 215, 255, 0.16);
    }
    body::after {
      width: 300px;
      height: 300px;
      right: -90px;
      bottom: -80px;
      background: rgba(255, 107, 107, 0.10);
      animation-direction: reverse;
    }
    @keyframes float {
      0%, 100% { transform: translate3d(0, 0, 0) scale(1); }
      50% { transform: translate3d(0, 18px, 0) scale(1.05); }
    }
    .wrap {
      position: relative;
      max-width: 1120px;
      margin: 0 auto;
      padding: 24px;
    }
    .panel {
      position: relative;
      background: linear-gradient(180deg, rgba(15, 23, 42, 0.92), rgba(8, 14, 27, 0.88));
      border: 1px solid var(--stroke);
      border-radius: 28px;
      padding: 22px;
      box-shadow: var(--shadow);
      backdrop-filter: blur(18px);
      overflow: hidden;
    }
    .panel::after {
      content: "";
      position: absolute;
      inset: 0;
      background: radial-gradient(circle at top right, rgba(99, 215, 255, 0.10), transparent 32%);
      pointer-events: none;
    }
    .topbar {
      position: relative;
      z-index: 1;
      display: flex;
      justify-content: space-between;
      align-items: flex-start;
      gap: 18px;
      flex-wrap: wrap;
      margin-bottom: 18px;
    }
    .eyebrow {
      margin-bottom: 8px;
      color: var(--cyan);
      font-size: 12px;
      font-weight: 700;
      letter-spacing: 0.24em;
      text-transform: uppercase;
    }
    h1 {
      margin: 0;
      font-size: clamp(30px, 5vw, 54px);
      line-height: 1.02;
      letter-spacing: -0.03em;
    }
    .subtitle {
      max-width: 720px;
      margin-top: 12px;
      color: var(--muted);
      font-size: 15px;
      line-height: 1.6;
    }
    .live-pill {
      display: flex;
      align-items: center;
      gap: 12px;
      min-width: 170px;
      padding: 14px 16px;
      border-radius: 18px;
      background: rgba(255, 255, 255, 0.06);
      border: 1px solid var(--stroke);
    }
    .pulse {
      width: 12px;
      height: 12px;
      border-radius: 50%;
      background: var(--green);
      box-shadow: 0 0 18px rgba(57, 217, 138, 0.85);
      flex: none;
    }
    .live-pill strong {
      display: block;
      font-size: 12px;
      text-transform: uppercase;
      letter-spacing: 0.2em;
    }
    .live-pill span {
      display: block;
      margin-top: 2px;
      color: var(--muted);
      font-size: 12px;
    }
    .status-hero {
      position: relative;
      z-index: 1;
      margin-top: 10px;
      padding: 22px;
      border-radius: 24px;
      border: 1px solid var(--stroke);
      background:
        linear-gradient(135deg, rgba(255, 255, 255, 0.08), rgba(255, 255, 255, 0.03)),
        radial-gradient(circle at top right, rgba(99, 215, 255, 0.12), transparent 30%);
      overflow: hidden;
    }
    .status-label {
      margin-bottom: 12px;
      color: var(--muted);
      font-size: 12px;
      letter-spacing: 0.24em;
      text-transform: uppercase;
    }
    .status-value {
      display: flex;
      align-items: center;
      gap: 14px;
      flex-wrap: wrap;
      font-size: clamp(34px, 6vw, 74px);
      font-weight: 800;
      line-height: 1;
    }
    .status-dot {
      width: 18px;
      height: 18px;
      border-radius: 50%;
      flex: none;
    }
    .status-text {
      display: inline-flex;
      align-items: center;
      gap: 10px;
      padding: 10px 16px;
      border-radius: 999px;
      border: 1px solid rgba(255, 255, 255, 0.10);
      background: rgba(255, 255, 255, 0.05);
      font-size: 14px;
      letter-spacing: 0.14em;
      text-transform: uppercase;
    }
    .status-subtitle {
      margin-top: 12px;
      color: var(--muted);
      font-size: 13px;
      line-height: 1.6;
    }
    .status-ok .status-dot { background: var(--green); box-shadow: 0 0 22px rgba(57, 217, 138, 0.75); }
    .status-ok .status-text { color: var(--green); }
    .status-warn .status-dot { background: var(--yellow); box-shadow: 0 0 22px rgba(245, 196, 81, 0.75); }
    .status-warn .status-text { color: var(--yellow); }
    .status-danger .status-dot { background: var(--red); box-shadow: 0 0 22px rgba(255, 107, 107, 0.75); }
    .status-danger .status-text { color: var(--red); }
    .grid {
      position: relative;
      z-index: 1;
      margin-top: 18px;
      display: grid;
      grid-template-columns: repeat(12, 1fr);
      gap: 14px;
    }
    .card {
      grid-column: span 6;
      padding: 18px;
      border-radius: 22px;
      border: 1px solid var(--stroke);
      background: rgba(255, 255, 255, 0.04);
      box-shadow: inset 0 1px 0 rgba(255, 255, 255, 0.03);
    }
    .card.wide { grid-column: span 12; }
    .card h3 {
      margin: 0 0 12px;
      color: var(--muted);
      font-size: 12px;
      letter-spacing: 0.22em;
      text-transform: uppercase;
    }
    .metric {
      display: flex;
      justify-content: space-between;
      align-items: flex-end;
      gap: 12px;
    }
    .metric .value {
      font-size: 28px;
      font-weight: 800;
      line-height: 1;
    }
    .metric .value small {
      color: var(--muted);
      font-size: 14px;
      font-weight: 600;
    }
    .metric .hint {
      color: var(--muted);
      font-size: 13px;
      line-height: 1.5;
      text-align: right;
    }
    .bar {
      margin-top: 14px;
      height: 10px;
      border-radius: 999px;
      background: rgba(255, 255, 255, 0.07);
      overflow: hidden;
    }
    .bar > span {
      display: block;
      height: 100%;
      border-radius: inherit;
      background: linear-gradient(90deg, var(--green), var(--yellow), var(--red));
      width: 100%;
    }
    .chips {
      display: flex;
      flex-wrap: wrap;
      gap: 10px;
      margin-top: 14px;
    }
    .chip {
      padding: 8px 12px;
      border-radius: 999px;
      background: rgba(255, 255, 255, 0.05);
      border: 1px solid var(--stroke);
      color: var(--text);
      font-size: 12px;
      letter-spacing: 0.10em;
      text-transform: uppercase;
    }
    .chip strong { color: var(--cyan); }
    .footer {
      position: relative;
      z-index: 1;
      margin-top: 16px;
      color: var(--muted);
      font-size: 12px;
      text-align: center;
      letter-spacing: 0.08em;
    }
    @media (max-width: 760px) {
      .wrap { padding: 14px; }
      .panel { padding: 16px; border-radius: 22px; }
      .card { grid-column: span 12; }
      .metric { align-items: flex-start; flex-direction: column; }
      .metric .hint { text-align: left; }
      .live-pill { width: 100%; justify-content: flex-start; }
    }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="panel">
      <header class="topbar">
        <div>
          <div class="eyebrow">ESP32 Driver Guardian</div>
          <h1>Live Safety Dashboard</h1>
          <div class="subtitle">Monitoring real-time untuk status pengemudi, suhu kabin, jarak kendaraan, dan kestabilan sistem.</div>
        </div>
        <div class="live-pill">
          <div class="pulse"></div>
          <div>
            <strong>Live</strong>
            <span>Auto refresh 2s</span>
          </div>
        </div>
      </header>

      <section class="status-hero %OVERALL_CLASS%">
        <div class="status-label">Overall Status</div>
        <div class="status-value">
          <span class="status-dot"></span>
          <span class="status-text">%OVERALL_STATUS%</span>
        </div>
        <div class="status-subtitle">%OVERALL_HINT%</div>
      </section>

      <div class="grid">
        <div class="card">
          <h3>Driver Mode</h3>
          <div class="metric">
            <div class="value %DRIVER_CLASS%">%DRIVER_STATUS%</div>
            <div class="hint">State pengemudi saat ini</div>
          </div>
        </div>

        <div class="card">
          <h3>Score</h3>
          <div class="metric">
            <div class="value %SCORE_CLASS%">%DRIVER_SCORE%<small>/100</small></div>
            <div class="hint">Semakin tinggi semakin aman</div>
          </div>
          <div class="bar"><span style="width:%DRIVER_SCORE%%"></span></div>
        </div>

        <div class="card">
          <h3>Cabin Temperature</h3>
          <div class="metric">
            <div class="value %TEMP_CLASS%">%CABIN_TEMP%<small>C</small></div>
            <div class="hint">Suhu kabin terdeteksi</div>
          </div>
        </div>

        <div class="card">
          <h3>Distance</h3>
          <div class="metric">
            <div class="value %DIST_CLASS%">%DIST_CM%<small>cm</small></div>
            <div class="hint">Jarak objek di depan sensor</div>
          </div>
        </div>

        <div class="card wide">
          <h3>Live Snapshot</h3>
          <div class="chips">
            <div class="chip">Humidity: <strong>%CABIN_HUM%</strong>%</div>
            <div class="chip">Unstable: <strong>%UNSTABLE%</strong></div>
            <div class="chip">WiFi: <strong>%WIFI%</strong></div>
            <div class="chip">Buzzer: <strong>%MUTED%</strong></div>
          </div>
        </div>
      </div>

      <div class="footer">
        System v2.0 | ESP32 Driver Guardian | Last update: <span id="timestamp"></span>
      </div>
    </div>
  </div>

  <script>
    document.getElementById('timestamp').textContent = new Date().toLocaleTimeString();
  </script>
</body>
</html>
)rawliteral";

  html.replace("%OVERALL_CLASS%", overallClass);
  html.replace("%OVERALL_STATUS%", overallStatus);
  html.replace("%OVERALL_HINT%", overallHint);
  html.replace("%DRIVER_CLASS%", driverClass);
  html.replace("%DRIVER_STATUS%", driverStatus);
  html.replace("%SCORE_CLASS%", scoreClass);
  html.replace("%DRIVER_SCORE%", String(driverScore));
  html.replace("%TEMP_CLASS%", tempClass);
  html.replace("%CABIN_TEMP%", String(cabinTemp, 1));
  html.replace("%DIST_CLASS%", distanceClass);
  html.replace("%DIST_CM%", String(distanceCM, 0));
  html.replace("%CABIN_HUM%", String(cabinHum, 0));
  html.replace("%UNSTABLE%", String(unstableVehicle ? "YES" : "NO"));
  html.replace("%WIFI%", String(wifiConnected ? "ON" : "OFF"));
  html.replace("%MUTED%", String(alarmMuted ? "MUTED" : "ACTIVE"));

  return html;
}

void handleRoot() {
  server.send(200, "text/html", htmlPageModern());
}

void handleNotFound() {
  String message = "404: Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  server.send(404, "text/plain", message);
}

void handleStatus() {
  String json = "{";
  json += "\"driver\":\"" + driverStatus + "\",";
  json += "\"status\":\"" + overallStatus + "\",";
  json += "\"score\":" + String(driverScore) + ",";
  json += "\"temp\":" + String(cabinTemp, 1) + ",";
  json += "\"hum\":" + String(cabinHum, 0) + ",";
  json += "\"distance\":" + String(distanceCM, 0) + ",";
  json += "\"unstable\":" + String(unstableVehicle ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

// =======================
// WIFI SETUP
// =======================

void setupWiFi() {
  Serial.println("\n📡 Connecting to WiFi...");
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("WiFi...");
  
  // Keep AP available as a fallback while trying STA.
  WiFi.setSleep(false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ap_ssid, ap_password);
  WiFi.begin(sta_ssid, sta_password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
    
    lcd.setCursor(0,1);
    lcd.print("Attempt ");
    lcd.print(attempts);
    lcd.print("/20");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("\n✅ WiFi Connected!");
    Serial.print("📡 IP Address: ");
    Serial.println(WiFi.localIP());
    
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("WiFi OK");
    lcd.setCursor(0,1);
    lcd.print(WiFi.localIP().toString().substring(0,16));
    delay(3000);
    return;
  }
  
  // If STA fails, stay on AP mode so the web server is still reachable.
  Serial.println("\n❌ STA Mode Failed!");
  Serial.println("💡 Starting AP Mode...");

  IPAddress apIP = WiFi.softAPIP();
  Serial.print("📡 AP IP: ");
  Serial.println(apIP);
  Serial.println("🔑 Password: 12345678");
  wifiConnected = true;
  
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("AP Mode");
  lcd.setCursor(0,1);
  lcd.print(apIP.toString().substring(0,16));
  delay(3000);
}

// =======================
// SETUP
// =======================

void setup() {
  Serial.begin(115200);
  Serial.println("\n🚗 DRIVER GUARDIAN SYSTEM v2.0");
  Serial.println("=================================");
  Serial.println("📡 WOKWI PORT FORWARD: localhost:8180");
  Serial.println("=================================");
  
  // Initialize pins
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Initialize LCD
  lcd.init();
  lcd.backlight();
  
  // LCD Splash
  lcd.setCursor(0,0);
  lcd.print("Smart Driver");
  lcd.setCursor(0,1);
  lcd.print("Guardian");
  delay(2000);
  
  // Initialize DHT
  dht.begin();
  delay(1000);
  
  // Initialize MPU6050
  if (!mpu.begin()) {
    Serial.println("⚠️ MPU6050 NOT FOUND - Continuing anyway");
  } else {
    Serial.println("✅ MPU6050 OK");
  }
  
  // Initialize Servo
  alarmServo.attach(SERVO_PIN);
  alarmServo.write(90);
  
  // Setup WiFi
  setupWiFi();
  
  // Setup Web Server
  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("✅ HTTP Server started");
  
  if (WiFi.getMode() == WIFI_AP) {
    Serial.println("=================================");
    Serial.println("🌐 Connect to WiFi: " + String(ap_ssid));
    Serial.println("🔑 Password: " + String(ap_password));
    Serial.println("🌐 Open browser: http://" + WiFi.softAPIP().toString());
    Serial.println("=================================");
  } else {
    Serial.println("=================================");
    Serial.println("🌐 Open browser: http://" + WiFi.localIP().toString());
    Serial.println("=================================");
  }
  
  // Show initial page
  showPage();
}

// =======================
// LOOP
// =======================

void loop() {
  // Handle web client
  server.handleClient();
  
  // ==================
  // BUTTON RESET
  // ==================
  
  if (digitalRead(BUTTON_PIN) == LOW) {
    alarmMuted = true;
    stopBuzzer();
    delay(200);
  }
  
  // ==================
  // SENSOR READ
  // ==================
  
  int potValue = analogRead(POT_PIN);
  
  float tempRead = dht.readTemperature();
  float humRead = dht.readHumidity();
  
  if (!isnan(tempRead)) {
    cabinTemp = tempRead;
  }
  if (!isnan(humRead)) {
    cabinHum = humRead;
  }
  
  distanceCM = readDistance();
  
  sensors_event_t a, g, temp;
  if (mpu.getEvent(&a, &g, &temp)) {
    unstableVehicle = false;
    if (abs(a.acceleration.x) > 6 ||
        abs(a.acceleration.y) > 6 ||
        abs(g.gyro.z) > 2.5) {
      unstableVehicle = true;
    }
  }
  
  // ==================
  // DRIVER STATUS
  // ==================
  
  if (potValue < 1500) {
    driverStatus = "FOCUS";
  } else if (potValue < 3000) {
    driverStatus = "DROWSY";
  } else {
    driverStatus = "SLEEPING";
  }
  
  // ==================
  // DRIVER SCORE
  // ==================
  
  driverScore = 100;
  if (driverStatus == "DROWSY") driverScore -= 20;
  if (driverStatus == "SLEEPING") driverScore -= 50;
  if (distanceCM < 50) driverScore -= 20;
  if (cabinTemp > 35) driverScore -= 10;
  if (unstableVehicle) driverScore -= 20;
  if (driverScore < 0) driverScore = 0;
  
  // ==================
  // OVERALL STATUS
  // ==================
  
  overallStatus = "AMAN";
  bool danger = false;
  
  if (driverStatus == "DROWSY" ||
      distanceCM < 80 ||
      cabinTemp > 30) {
    overallStatus = "WASPADA";
  }
  
  if (driverStatus == "SLEEPING" ||
      distanceCM < 50 ||
      unstableVehicle) {
    danger = true;
  }
  
  if (danger) overallStatus = "BAHAYA";
  
  // ==================
  // LED CONTROL
  // ==================
  
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED, LOW);
  
  if (overallStatus == "AMAN") {
    digitalWrite(LED_GREEN, HIGH);
  } else if (overallStatus == "WASPADA") {
    digitalWrite(LED_YELLOW, HIGH);
  } else {
    digitalWrite(LED_RED, HIGH);
  }
  
  // ==================
  // ALARM CONTROL
  // ==================
  
  if (overallStatus == "AMAN") {
    stopBuzzer();
    alarmServo.write(90);
    alarmMuted = false;
  } else if (overallStatus == "WASPADA") {
    if (!alarmMuted) beepWarning();
  } else {
    if (!alarmMuted) {
      beepDanger();
      shakeServo();
    }
  }
  
  // ==================
  // EMERGENCY MODE
  // ==================
  
  if (driverStatus == "SLEEPING") {
    if (!sleeping) {
      sleeping = true;
      sleepStartTime = millis();
    }
    
    if (millis() - sleepStartTime > 10000) {
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("EMERGENCY!");
      lcd.setCursor(0,1);
      lcd.print("STOP VEHICLE");
      digitalWrite(LED_RED, HIGH);
      if (!alarmMuted) beepDanger();
      shakeServo();
    }
  } else {
    sleeping = false;
  }
  
  // ==================
  // LCD AUTO PAGE
  // ==================
  
  if (millis() - lastPageChange > 3000) {
    currentPage++;
    if (currentPage > 3) currentPage = 0;
    showPage();
    lastPageChange = millis();
  }
  
  // ==================
  // SERIAL MONITOR
  // ==================
  
  static unsigned long lastSerialPrint = 0;
  if (millis() - lastSerialPrint > 3000) {
    Serial.println("=================================");
    if (WiFi.getMode() == WIFI_AP) {
      Serial.print("📡 AP IP: ");
      Serial.println(WiFi.softAPIP());
    } else {
      Serial.print("📡 IP: ");
      Serial.println(WiFi.localIP());
    }
    Serial.print("👤 Driver: ");
    Serial.println(driverStatus);
    Serial.print("📊 Status: ");
    Serial.println(overallStatus);
    Serial.print("🎯 Score: ");
    Serial.println(driverScore);
    Serial.print("🌡️ Temp: ");
    Serial.println(cabinTemp);
    Serial.print("💧 Humidity: ");
    Serial.println(cabinHum);
    Serial.print("📏 Distance: ");
    Serial.println(distanceCM);
    Serial.print("🚗 Unstable: ");
    Serial.println(unstableVehicle ? "YES" : "NO");
    Serial.println("=================================");
    Serial.println("🌐 http://localhost:8180");
    Serial.println("=================================");
    lastSerialPrint = millis();
  }
  
  delay(100);
}
