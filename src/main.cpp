#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include "time.h"

// 1. Cấu hình Wi-Fi & MQTT Broker (Thay bằng thông số thực tế)
const char* ssid        = "YOUR_WIFI_SSID";
const char* password    = "YOUR_WIFI_PASSWORD";
const char* mqtt_server = "192.168.0.188"; // Thay bằng IP LAN của Pi 4
const int   mqtt_port   = 1883;
const char* mqtt_topic  = "vehicle/01/telemetry";

// 2. Cấu hình NTP (Giờ UTC)
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 0;

// 3. Khởi tạo ngoại vi
DHT dht(4, DHT11);
LiquidCrystal_I2C lcd(0x27, 16, 2); // Đổi thành 0x3F nếu mạch dùng 0x3F
WiFiClient espClient;
PubSubClient client(espClient);

// 4. Biến định danh & định thời không chặn
uint32_t boot_id = 0;
uint32_t seq = 0;
unsigned long lastPublishMs = 0;

// Biến Exponential Backoff (1s -> 2s -> 4s -> 8s -> 16s -> tối đa 30s)
unsigned long lastReconnectAttempt = 0;
unsigned long backoffMs = 1000;
uint32_t reconnectCount = 0;

String getUTCTimeString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "1970-01-01T00:00:00Z";
  }
  char buf[25];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
  return String(buf);
}

void setupWiFi() {
  lcd.setCursor(0, 0);
  lcd.print("Ket noi Wi-Fi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[WiFi] Connected! IP: " + WiFi.localIP().toString());
}

// Hàm kết nối lại MQTT không chặn (Non-blocking)
bool tryReconnectMQTT() {
  String clientId = "ESP32_Vehicle01_" + String(boot_id);
  Serial.printf("[MQTT] Dang ket noi toi %s (backoff = %lu ms)...\n", mqtt_server, backoffMs);

  if (client.connect(clientId.c_str())) {
    Serial.println("[MQTT] Ket noi thanh cong!");
    backoffMs = 1000; // Reset backoff về 1s khi thành công
    return true;
  } else {
    Serial.printf("[MQTT] That bai! client.state() = %d\n", client.state());
    backoffMs = min(backoffMs * 2, 30000UL); // Nhân đôi thời gian chờ, tối đa 30s
    return false;
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  dht.begin();
  lcd.init();
  lcd.backlight();

  randomSeed(esp_random());
  boot_id = random(100000, 999999);

  setupWiFi();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  client.setServer(mqtt_server, mqtt_port);
  lcd.clear();
}

void loop() {
  unsigned long now = millis();

  // Kiểm tra Wi-Fi & duy trì kết nối MQTT không chặn
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      if (now - lastReconnectAttempt >= backoffMs) {
        lastReconnectAttempt = now;
        reconnectCount++;
        if (tryReconnectMQTT()) {
          lastReconnectAttempt = 0;
        }
      }
    } else {
      client.loop(); // Bắt buộc gọi đều để duy trì gói tin MQTT
    }
  }

  // Publish bản tin mỗi 1 giây (1000 ms)
  if (now - lastPublishMs >= 1000) {
    lastPublishMs = now;
    seq++;

    float h = dht.readHumidity();
    float t = dht.readTemperature();
    long rssi = WiFi.RSSI();
    String utcTime = getUTCTimeString();

    // Hiển thị LCD
    lcd.setCursor(0, 0);
    if (isnan(h) || isnan(t)) {
      lcd.print("Loi doc DHT11!  ");
    } else {
      lcd.print("T:"); lcd.print(t, 1); lcd.print("C H:"); lcd.print(h, 0); lcd.print("%   ");
    }
    lcd.setCursor(0, 1);
    lcd.print(client.connected() ? "MQTT:OK " : "MQTT:ERR");
    lcd.print(" R:"); lcd.print(rssi);

    // Đóng gói JSON
    char payload[180];
    snprintf(payload, sizeof(payload),
             "{\"boot_id\":%u,\"seq\":%u,\"utc\":\"%s\",\"temp\":%.1f,\"hum\":%.1f,\"rssi\":%ld,\"reconn\":%u}",
             boot_id, seq, utcTime.c_str(), t, h, rssi, reconnectCount);

    // Gửi lên MQTT và kiểm tra lỗi
    if (client.connected()) {
      bool ok = client.publish(mqtt_topic, payload);
      if (ok) {
        Serial.printf("[PUB OK] %s\n", payload);
      } else {
        Serial.printf("[PUB FAIL] state=%d | %s\n", client.state(), payload);
      }
    } else {
      Serial.printf("[OFFLINE] Chua ket noi Broker | %s\n", payload);
    }
  }
}