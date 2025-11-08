#include <esp_now.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// -------------------- CONFIG --------------------
#define BIN_HEIGHT 45   // cm

// WiFi credentials (must match sender’s WiFi to align channels)
const char* ssid = "SSID";
const char* password = "PASSWORD";

// Sender ESP32-CAM MAC Address (replace with actual MAC)
uint8_t senderMacAddress[] = {0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX};

// I2C LCD pins
const int SDA_PIN = 16;
const int SCL_PIN = 17;

// Bin Sensor 1
#define TRIG1 5
#define ECHO1 18
// Bin Sensor 2
#define TRIG2 19
#define ECHO2 21
// Bin Sensor 3
#define TRIG3 4
#define ECHO3 2

// Servo pins
int rotatePin = 22;   // rotation servo
int tiltPin   = 23;   // tilt servo

// Button pins
#define BTN1 32   // Bin1 (CW + Tilt, Paper)
#define BTN2 33   // Bin2 (Tilt only, Plastic)
#define BTN3 25   // Bin3 (CCW + Tilt, Can)

// -------------------- OBJECTS --------------------
WebServer server(80);
Servo servoRotate;
Servo servoTilt;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// -------------------- ESP-NOW CALLBACK --------------------
void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
  char* receivedData = (char*)data;
  String predictedClass = String(receivedData, len);
  Serial.println("Received class: " + predictedClass);

  // Map received class to bin actions
  if (predictedClass == "paper") {
    rotateBin1(); // CW + Tilt for Paper
    Serial.println("Paper Detected!");
    //lcd.clear();
    //lcd.setCursor(0, 0);
    //lcd.print("Paper Detected!");
  } else if (predictedClass == "plastic") {
    rotateBin2(); // Tilt only for Plastic
    Serial.println("Plastic Detected!");
    //lcd.clear();
    //lcd.setCursor(0, 0);
    //lcd.print("Plastic Detected!");
  } else if (predictedClass == "can") {
    rotateBin3(); // CCW + Tilt for Can
    Serial.println("Metal Detected!");
    //lcd.clear();
    //lcd.setCursor(0, 0);
    //lcd.print("Metal Detected!");
  } else {
    Serial.println("Item Cannot Detected!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Item Cannot Detected!");
  }
}

// -------------------- SENSOR FUNCTIONS --------------------
long readDistanceCM(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000);
  long distance = duration * 0.034 / 2;
  return distance;
}

void smoothRotate(Servo &servo, int startAngle, int endAngle, int stepDelay) {
  int step = (startAngle < endAngle) ? 1 : -1;
  for (int pos = startAngle; pos != endAngle; pos += step) {
    servo.write(pos);
    delay(stepDelay);
  }
  servo.write(endAngle);
}

int getFillPercent(long distance) {
  if (distance > BIN_HEIGHT) return 0;
  int percent = (int)((BIN_HEIGHT - distance) * 100 / BIN_HEIGHT);
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  return percent;
}

// -------------------- ROTATION ACTIONS --------------------
void rotateBin1() { // CW + Tilt (Paper)
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Paper Detected!");
  servoRotate.write(80);
  delay(500);
  servoRotate.write(92);

  smoothRotate(servoTilt, 0, 60, 15);
  delay(500);
  smoothRotate(servoTilt, 60, 0, 15);

  servoRotate.write(104);
  delay(500);
  servoRotate.write(92);

  Serial.println("Rotate CW 120° and Tilt (Bin1 - Paper)");
}

void rotateBin2() { // Tilt only (Plastic)
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Plastic Detected!");
  smoothRotate(servoTilt, 0, 60, 15);
  delay(500);
  smoothRotate(servoTilt, 60, 0, 15);

  Serial.println("Tilt (Bin2 - Plastic)");
}

void rotateBin3() { // CCW + Tilt (Can)
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Metal Detected!");
  servoRotate.write(104);
  delay(500);
  servoRotate.write(92);

  smoothRotate(servoTilt, 0, 60, 15);
  delay(500);
  smoothRotate(servoTilt, 60, 0, 15);

  servoRotate.write(80);
  delay(500);
  servoRotate.write(92);

  Serial.println("Rotate CCW 120° and Tilt (Bin3 - Can)");
}

// -------------------- HTML PAGE --------------------
String htmlPage() {
  String page = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>ESP32 Bin & Camera Control</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      text-align: center;
      background: #292827;
      margin: 0;
      padding: 0;
    }
    header {
      background: #2c3e50;
      color: white;
      padding: 15px;
    }
    h2 {
      margin: 15px 0;
    }
    .container {
      display: flex;
      flex-wrap: wrap;
      justify-content: center;
      gap: 20px;
      margin: 20px;
    }
    .bin-box {
      width: 100px;
      height: 180px;
      border: 3px solid #2c3e50;
      border-radius: 8px;
      position: relative;
      overflow: hidden;
      background: #f0f0f0;
      margin: auto;
    }
    .fill {
      position: absolute;
      bottom: 0;
      left: 0;
      width: 100%;
      height: 0%;
      background: linear-gradient(to top, #27ae60, #2ecc71);
      transition: height 0.5s;
    }
    .bin-label {
      margin-top: 10px;
      font-weight: bold;
      font-size: 16px;
      color: #2c3e50;
    }
    .card {
      background: white;
      padding: 20px;
      border-radius: 12px;
      box-shadow: 0 4px 8px rgba(0,0,0,0.1);
      width: 280px;
    }
    .btn {
      display: block;
      width: 100%;
      padding: 12px;
      margin: 10px 0;
      font-size: 16px;
      border: none;
      border-radius: 8px;
      cursor: pointer;
      color: white;
      transition: opacity 0.2s;
    }
    .btn:hover {
      opacity: 0.9;
    }
    .btn-bin1 { background: #3498db; }
    .btn-bin2 { background: #27ae60; }
    .btn-bin3 { background: #e74c3c; }
    footer {
      margin: 20px;
      font-size: 14px;
      color: #FFFFFF;
    }
  </style>
  <script>
    function updateData(){
      fetch('/data').then(r=>r.json()).then(d=>{
        document.getElementById('fill1').style.height = d.b1 + '%';
        document.getElementById('fill2').style.height = d.b2 + '%';
        document.getElementById('fill3').style.height = d.b3 + '%';
        document.getElementById('label1').innerText = "Paper: " + d.b1 + "%";
        document.getElementById('label2').innerText = "Plastic: " + d.b2 + "%";
        document.getElementById('label3').innerText = "Can: " + d.b3 + "%";
      });
    }
    setInterval(updateData,2000);
  </script>
</head>
<body onload="updateData()">
  <header>
    <h1>ESP32 Smart Bin Monitor</h1>
  </header>
  <div class="container">
    <div class="card">
      <h2>Bin Levels</h2>
      <div class="container">
        <div>
          <div class="bin-box"><div id="fill1" class="fill"></div></div>
          <div id="label1" class="bin-label">Paper: ...</div>
        </div>
        <div>
          <div class="bin-box"><div id="fill2" class="fill"></div></div>
          <div id="label2" class="bin-label">Plastic: ...</div>
        </div>
        <div>
          <div class="bin-box"><div id="fill3" class="fill"></div></div>
          <div id="label3" class="bin-label">Can: ...</div>
        </div>
      </div>
    </div>
    <div class="card">
      <h2>Camera Mount Control</h2>
      <button class="btn btn-bin1" onclick="fetch('/rotate120?dir=CW')">Move to Paper</button>
      <button class="btn btn-bin2" onclick="fetch('/rotate120?dir=W')">Move to Plastic</button>
      <button class="btn btn-bin3" onclick="fetch('/rotate120?dir=CCW')">Move to Can</button>
    </div>
  </div>
  <footer>
    ESP32 Bin & Camera Controller © 2025
  </footer>
</body>
</html>
  )rawliteral";
  return page;
}

// -------------------- ROUTES --------------------
void handleRoot() {
  server.send(200, "text/html", htmlPage());
}

void handleData() {
  long d1 = readDistanceCM(TRIG1, ECHO1);
  long d2 = readDistanceCM(TRIG2, ECHO2);
  long d3 = readDistanceCM(TRIG3, ECHO3);

  int p1 = getFillPercent(d1);
  int p2 = getFillPercent(d2);
  int p3 = getFillPercent(d3);

  String json = "{\"b1\":" + String(p1) + ",\"b2\":" + String(p2) + ",\"b3\":" + String(p3) + "}";
  server.send(200, "application/json", json);
}

void handleRotate120() {
  if (server.hasArg("dir")) {
    String dir = server.arg("dir");

    if (dir == "CW") rotateBin1();
    else if (dir == "W") rotateBin2();
    else if (dir == "CCW") rotateBin3();
  }
  server.send(200, "text/plain", "Rotation Done");
}

// -------------------- SETUP --------------------
void setup() {
  Serial.begin(9600);
  delay(1000);  // Wait for Serial to stabilize
  Serial.println("Starting ESP32 Smart Bin Receiver...");

  // Initialize I2C
  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();

  // Initialize pins
  pinMode(TRIG1, OUTPUT); pinMode(ECHO1, INPUT);
  pinMode(TRIG2, OUTPUT); pinMode(ECHO2, INPUT);
  pinMode(TRIG3, OUTPUT); pinMode(ECHO3, INPUT);
  pinMode(BTN1, INPUT_PULLUP);
  pinMode(BTN2, INPUT_PULLUP);
  pinMode(BTN3, INPUT_PULLUP);

  // Attach servos
  servoRotate.attach(rotatePin, 500, 2400);
  servoTilt.attach(tiltPin, 500, 2400);

  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected, IP: " + WiFi.localIP().toString());
  Serial.println("RSSI: " + String(WiFi.RSSI()));
  Serial.println("WiFi Channel: " + String(WiFi.channel()));

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    while (1);
  }
  Serial.println("ESP-NOW initialized");

  // Register receive callback
  esp_now_register_recv_cb(OnDataRecv);

  // Add sender as peer
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, senderMacAddress, 6);
  peerInfo.channel = WiFi.channel();
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;
  Serial.print("Adding ESP-NOW peer: ");
  for (int i = 0; i < 6; i++) {
    Serial.print(senderMacAddress[i], HEX);
    if (i < 5) Serial.print(":");
  }
  Serial.println();
  Serial.println("Peer channel: " + String(peerInfo.channel));
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("ESP-NOW peer add failed");
    while (1);
  }
  Serial.println("ESP-NOW peer added");

  // Start web server
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/rotate120", handleRotate120);
  server.begin();
  Serial.println("Web server started");
  Serial.println("Setup complete");
}

// -------------------- LOOP --------------------
void loop() {
  server.handleClient();

  // Button press handling (active LOW)
  if (digitalRead(BTN1) == LOW) {
    rotateBin1();
    delay(500); // debounce delay
  }
  if (digitalRead(BTN2) == LOW) {
    rotateBin2();
    delay(500);
  }
  if (digitalRead(BTN3) == LOW) {
    rotateBin3();
    delay(500);
  }

  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, reconnecting...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("Reconnected to WiFi");
  }
}
