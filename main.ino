#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>

// Pin Definitions - Fixed for ESP32
#define DHTPIN 4         // DHT11 data pin connected to GPIO4
#define MQ_ANALOG_PIN 34 // MQ-2 analog output connected to GPIO34 (ADC1_6)
#define MQ_DIGITAL_PIN 5 // MQ-2 digital output connected to GPIO5
#define BUZZER_PIN 2     // Buzzer connected to GPIO2

// Sensor type
#define DHTTYPE DHT11

// Safety thresholds - Adjusted for ESP32 ADC range (0-4095)
#define MAX_TEMP 35.0
#define MIN_HUMIDITY 20.0
#define GAS_ALARM_THRESHOLD 1000  // Adjusted for ESP32's 12-bit ADC

// Buzzer configuration
#define BUZZER_FREQUENCY 2000

// Network credentials
const char* ssid = "WE_8CA4D2";
const char* password = "j9n04550!2";

DHT dht(DHTPIN, DHTTYPE);
WebServer server(80);

void setup() {
  Serial.begin(115200);
  Serial.println("\nStarting safety monitoring system...");
  
  initializeSensors();
  connectToWiFi();
  setupWebServer();
  
  Serial.println("System initialization complete!");
}

void loop() {
  server.handleClient();
  
  // Debug output every 5 seconds to verify sensor readings
  static unsigned long lastDebugTime = 0;
  if (millis() - lastDebugTime > 5000) {
    lastDebugTime = millis();
    debugSensorValues();
  }
}

void initializeSensors() {
  Serial.println("Initializing sensors...");
  
  // Initialize DHT sensor
  dht.begin();
  
  // Configure pins
  pinMode(MQ_DIGITAL_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  
  // Test buzzer
  Serial.println("Testing buzzer...");
  tone(BUZZER_PIN, BUZZER_FREQUENCY);
  delay(500);
  noTone(BUZZER_PIN);
  
  Serial.println("Waiting for MQ-2 sensor warm-up (20 seconds)...");
  // Warm-up delay for MQ-2
  for (int i = 20; i > 0; i--) {
    Serial.printf("MQ-2 warm-up: %d seconds remaining...\n", i);
    delay(1000);
  }
  
  Serial.println("Sensors initialized!");
}

void connectToWiFi() {
  Serial.printf("Connecting to WiFi network: %s\n", ssid);
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected successfully!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi connection failed! Operating in offline mode.");
  }
}

void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/data", handleSensorData);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("Web server started");
}

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="ar">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>لوحة مراقبة الحساسات</title>
  <style>
    body {
      margin: 0;
      padding: 0;
      font-family: 'Cairo', sans-serif;
      background: linear-gradient(135deg, #0f2027, #203a43, #2c5364);
      color: #ffffff;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: start;
      min-height: 100vh;
      padding: 20px;
    }
    h1 {
      margin: 20px 0;
      font-size: 2em;
      text-align: center;
    }
    .dashboard {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
      gap: 20px;
      width: 100%;
      max-width: 900px;
      margin-top: 20px;
    }
    .card {
      background: #1e293b;
      border-radius: 15px;
      padding: 30px;
      box-shadow: 0 4px 15px rgba(0,0,0,0.3);
      text-align: center;
      transition: transform 0.3s;
    }
    .card:hover {
      transform: translateY(-5px);
    }
    .icon {
      font-size: 3em;
      margin-bottom: 15px;
    }
    .value {
      font-size: 2.5em;
      margin: 10px 0;
    }
    .label {
      color: #b0bec5;
      font-size: 1em;
    }
    .unsafe {
      background: #ff5252;
      animation: blink 1s infinite;
    }
    @keyframes blink {
      0% { opacity: 1; }
      50% { opacity: 0.6; }
      100% { opacity: 1; }
    }
    .alert-banner {
      background-color: #ff5252;
      color: white;
      font-size: 1.5em;
      padding: 10px;
      width: 100%;
      text-align: center;
      display: none;
      animation: blink 1s infinite;
      position: fixed;
      top: 0;
      left: 0;
      z-index: 100;
    }
    .refresh-button {
      background-color: #4CAF50;
      color: white;
      border: none;
      padding: 10px 20px;
      font-size: 1.2em;
      cursor: pointer;
      border-radius: 5px;
      margin-top: 20px;
    }
    .refresh-button:hover {
      background-color: #45a049;
    }
  </style>
</head>
<body>
  <h1>🛡️ لوحة مراقبة الحساسات</h1>
  <div class="alert-banner" id="alertBanner">تحذير: حالة غير آمنة!</div>

  <div class="dashboard">
    <div class="card" id="tempCard">
      <div class="icon">🌡️</div>
      <div class="value" id="temp">-- °C</div>
      <div class="label">درجة الحرارة</div>
    </div>
    <div class="card" id="humCard">
      <div class="icon">💧</div>
      <div class="value" id="hum">-- %</div>
      <div class="label">الرطوبة</div>
    </div>
    <div class="card" id="gasCard">
      <div class="icon">💨</div>
      <div class="value" id="gas">--</div>
      <div class="label">مستوى الغاز</div>
    </div>
  </div>

  <button class="refresh-button" onclick="fetchData()">تحديث الآن</button>

  <script>
    async function fetchData() {
      try {
        const response = await fetch('/data');
        if (!response.ok) throw new Error('Network error');
        
        const data = await response.json();
        
        // Update the values on the page
        document.getElementById('temp').textContent = data.temp + " °C";
        document.getElementById('hum').textContent = data.hum + " %";
        document.getElementById('gas').textContent = data.gas;

        // Check for unsafe conditions
        const unsafe = data.tempAlarm || data.humAlarm || data.gasAlarm;
        
        // Update card styles
        document.getElementById('tempCard').classList.toggle('unsafe', data.tempAlarm);
        document.getElementById('humCard').classList.toggle('unsafe', data.humAlarm);
        document.getElementById('gasCard').classList.toggle('unsafe', data.gasAlarm);

        // Show/hide alert banner
        document.getElementById('alertBanner').style.display = unsafe ? 'block' : 'none';
        
      } catch (error) {
        console.error('Error fetching data:', error);
      }
    }

    // Fetch data every 3 seconds automatically
    setInterval(fetchData, 3000);
    fetchData(); // Initial fetch when the page loads
  </script>
</body>
</html>
  )rawliteral";

  server.send(200, "text/html", html);
}

void handleSensorData() {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  int gasValue = analogRead(MQ_ANALOG_PIN);
  bool gasDigitalValue = digitalRead(MQ_DIGITAL_PIN) == LOW; // LOW means gas detected

  // Check if sensor readings are valid
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    humidity = 0;
    temperature = 0;
  }

  // Check safety thresholds
  bool tempAlarm = temperature > MAX_TEMP;
  bool humAlarm = humidity < MIN_HUMIDITY;
  bool gasAlarm = gasValue > GAS_ALARM_THRESHOLD || gasDigitalValue;
  bool anyAlarm = tempAlarm || humAlarm || gasAlarm;

  // Control buzzer
  if (anyAlarm) {
    tone(BUZZER_PIN, BUZZER_FREQUENCY);
  } else {
    noTone(BUZZER_PIN);
  }

  // Prepare JSON response
  String json = "{";
  json += "\"temp\":" + String(temperature, 1) + ",";
  json += "\"tempAlarm\":" + String(tempAlarm) + ",";
  json += "\"hum\":" + String(humidity, 1) + ",";
  json += "\"humAlarm\":" + String(humAlarm) + ",";
  json += "\"gas\":" + String(gasValue) + ",";
  json += "\"gasAlarm\":" + String(gasAlarm);
  json += "}";

  server.send(200, "application/json", json);
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Not found");
}

void debugSensorValues() {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  int gasValue = analogRead(MQ_ANALOG_PIN);
  bool gasDigitalValue = digitalRead(MQ_DIGITAL_PIN) == LOW;
  
  Serial.println("---- DEBUG SENSOR VALUES ----");
  Serial.printf("Temperature: %.1f °C\n", temperature);
  Serial.printf("Humidity: %.1f %%\n", humidity);
  Serial.printf("Gas Value (Analog): %d\n", gasValue);
  Serial.printf("Gas Detected (Digital): %s\n", gasDigitalValue ? "YES" : "NO");
  Serial.println("----------------------------");
}