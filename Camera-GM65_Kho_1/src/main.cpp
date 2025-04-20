#include <WiFi.h>
#include <PubSubClient.h>
#include "GM65_scanner.h"
#include <WiFiClientSecure.h>
#include <esp_task_wdt.h>
#include <DHT.h>
#include <ArduinoJson.h>

// WiFi & MQTT Configuration
const char* ssid = "IEC Local";
const char* password = "IEC@2023";
const char* mqtt_server = "7e44c054e3384872bc6b019a4185eb18.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_user = "iec-local";
const char* mqtt_pass = "Aa12345678";
const char* mqtt_topic = "esp32/data";

WiFiClientSecure espClient;
PubSubClient client(espClient);
GM65_scanner scanner(&Serial2);

// DHT11 Configuration
#define DHTPIN 23  // Change to your pin
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Flag for QR scan delay
bool qrScanned = false;
unsigned long lastScanTime = 0;
unsigned long scanDelay = 1000;

// Function to connect to WiFi
void setupWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

// Function to connect to MQTT Broker
void reconnectMQTT() {
  espClient.setInsecure();  // Bỏ qua chứng chỉ TLS
  client.setKeepAlive(60); // Timeout MQTT 60 giây
  espClient.setTimeout(60); // Timeout socket 60 giây

  while (!client.connected()) {
    Serial.print("Đang kết nối MQTT...");
    if (client.connect("ESP32_Client_1", mqtt_user, mqtt_pass)) {
      Serial.println("Đã kết nối MQTT!");
      client.subscribe("esp32/control");  // Nhận lệnh từ MQTT
    } else {
      Serial.print("Lỗi MQTT, mã lỗi = ");
      Serial.println(client.state());
      Serial.println("Thử lại sau 5 giây...");
      delay(5000);
    }
  }
}

// Function to initialize scanner
void initScanner() {
  Serial.println("Initializing scanner...");
  Serial2.begin(9600);
  scanner.init();
  Serial.println("Enabling setting QR code...");
  scanner.set_working_mode(3);
  scanner.enable_setting_code();
  Serial.println("Scanner initialized");
}

// Function to read QR code
String readQRCode() {
  return scanner.get_info();
}

// Function to read DHT11 sensor
void readDHT11(float &temperature, float &humidity) {
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  Serial.print("Temperature: "); Serial.print(temperature);
  Serial.print("°C, Humidity: "); Serial.print(humidity);
  Serial.println("%");
}

void setup() {
  Serial.begin(115200);
  setupWiFi();
  client.setServer(mqtt_server, mqtt_port);
  reconnectMQTT();
  initScanner();
  dht.begin();
  
  // Initialize Watchdog Timer (Timeout: 4 seconds)
  esp_task_wdt_init(4, true);
  esp_task_wdt_add(NULL);
}

void loop() {
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();
  String qrData = readQRCode();
  if(qrData.length() == 0) {
    Serial.println("No QR Code scanned");
  }
  else if (qrData.length() > 0) {
    float temperature, humidity;
    readDHT11(temperature, humidity);
    StaticJsonDocument<200> jsonDoc;
    jsonDoc["temperature"] = temperature;
    jsonDoc["humidity"] = humidity;
    jsonDoc["qr_code"] = qrData;
    Serial.println("QR Code scanned, delaying next scan for 1 seconds...");
    qrScanned = true;
    scanDelay = 200;
    char jsonBuffer[256];
    serializeJson(jsonDoc, jsonBuffer);
    client.publish(mqtt_topic, jsonBuffer);
    Serial.println(jsonBuffer);
    Serial.println("Published JSON Data to MQTT");
  }
  esp_task_wdt_reset();
  delay(1000);
}