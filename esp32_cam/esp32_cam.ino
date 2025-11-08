#include "esp_camera.h"
#include <WiFi.h>
#include "Base64.h"
#include <HTTPClient.h>
#include <esp_now.h>
#include <ArduinoJson.h>   // New library for parsing JSON

// WiFi credentials
const char* ssid = "SSID";
const char* password = "PASSWORD";

// Server details
const char* serverUrl = "http://X.X.X.X:5000/predict";  // PC’s IP

// ESP32-Receiver MAC Address
uint8_t receiverMacAddress[] = {0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX};

// Camera pins for ESP32-CAM (AI Thinker)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Sorting pin
#define SORT_PIN 2

// Ultrasonic pins
#define TRIG_PIN 12
#define ECHO_PIN 13

// ESP-NOW callback
void OnDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "ESP-NOW Sent" : "ESP-NOW Failed");
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected, IP: " + WiFi.localIP().toString());
  Serial.println("MAC Address: " + WiFi.macAddress());
  Serial.println("RSSI: " + String(WiFi.RSSI()));
  Serial.println("WiFi Channel: " + String(WiFi.channel()));
}

// Measure distance with ultrasonic
long getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // timeout 30ms
  long distance = duration * 0.034 / 2;
  return distance;
}

void setup() {
  Serial.begin(115200);
  pinMode(SORT_PIN, OUTPUT);

  // Ultrasonic setup
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Camera config
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_VGA;  // 160x120 for stability
  config.jpeg_quality = 20;
  config.fb_count = 2;

  // Init camera
  Serial.println("Initializing camera...");
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    while (1);
  }
  Serial.println("Camera initialized");

  // Connect WiFi
  connectWiFi();

  // Init ESP-NOW
  delay(1000);  // Ensure WiFi stability
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    while (1);
  }
  Serial.println("ESP-NOW initialized");
  esp_now_register_send_cb(OnDataSent);

  // Add peer
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, receiverMacAddress, 6);
  peerInfo.channel = WiFi.channel();
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;

  Serial.print("Adding ESP-NOW peer: ");
  for (int i = 0; i < 6; i++) {
    Serial.print(receiverMacAddress[i], HEX);
    if (i < 5) Serial.print(":");
  }
  Serial.println();
  Serial.println("Peer channel: " + String(peerInfo.channel));

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("ESP-NOW peer add failed");
    while (1);
  }
  Serial.println("ESP-NOW peer added");
}

void loop() {
  // Measure distance
  long distance = getDistance();
  //Serial.println("Distance: " + String(distance) + " cm");

  // Only run classification when object is 4–10 cm away
  if (distance >= 4 && distance <= 25) {
    Serial.println("Object detected in range! Capturing...");
    delay(500);
    // Capture image
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      delay(500);
      return;
    }

    // Base64 encode
    String img64 = "data:image/jpeg;base64," + base64::encode(fb->buf, fb->len);
    Serial.println("Image size: " + String(fb->len) + " bytes");

    // Send to server
    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(10000);

    String payload = "{\"image\":\"" + img64 + "\"}";
    int httpResponseCode = http.POST(payload);

    String predictedClass = "other";  // default
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Raw response: [" + response + "]");

      // Parse JSON safely
      StaticJsonDocument<256> doc;
      DeserializationError error = deserializeJson(doc, response);

      if (!error) {
        if (doc.containsKey("class")) {
          predictedClass = doc["class"].as<String>();
          Serial.println("Parsed class: " + predictedClass);
        } else {
          Serial.println("JSON missing 'class' field");
        }
      } else {
        Serial.println("JSON parse failed: " + String(error.c_str()));
      }

      // Validate class
      if (predictedClass != "paper" && 
          predictedClass != "plastic" && 
          predictedClass != "can" && 
          predictedClass != "other") {
        Serial.println("Invalid class received: " + predictedClass);
        predictedClass = "other";
      }

      Serial.println("Final Predicted: " + predictedClass);

      // Sorting action
      if (predictedClass == "paper") {
        digitalWrite(SORT_PIN, HIGH);
        delay(1000);
        digitalWrite(SORT_PIN, LOW);
      } else if (predictedClass == "plastic") {
        digitalWrite(SORT_PIN, HIGH);
        delay(1000);
        digitalWrite(SORT_PIN, LOW);
      } else if (predictedClass == "can") {
        digitalWrite(SORT_PIN, HIGH);
        delay(1000);
        digitalWrite(SORT_PIN, LOW);
      } else {
        digitalWrite(SORT_PIN, HIGH);
        delay(1000);
        digitalWrite(SORT_PIN, LOW);
      }

      // Send class to ESP32-Receiver via ESP-NOW
      const char* data = predictedClass.c_str();
      esp_err_t result = esp_now_send(receiverMacAddress, (uint8_t *)data, strlen(data));
      if (result == ESP_OK) {
        Serial.println("ESP-NOW sending class: " + predictedClass);
      } else {
        Serial.println("ESP-NOW send error: " + String(result));
      }
    } else {
      Serial.println("HTTP Error: " + String(httpResponseCode));
    }

    http.end();
    esp_camera_fb_return(fb);

    delay(5000); // wait before next detection
  }

  delay(500);
}
