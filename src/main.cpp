#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include "time.h"

// Cấu hình Wi-Fi & NTP
const char* ssid = "dmi";
const char* password = "20082008";
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;      // Giờ UTC (GMT+0)
const int   daylightOffset_sec = 0;

// Phần cứng
DHT dht(4, DHT11);
LiquidCrystal_I2C lcd(0x27, 16, 2); // Đổi thành 0x3F nếu mạch của bạn dùng 0x3F

// Biến định danh bản tin Tuần 3
uint32_t boot_id = 0;
uint32_t seq = 0;

String getUTCTimeString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "1970-01-01T00:00:00Z";
  }
  char buf[25];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
  return String(buf);
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  dht.begin();
  lcd.init();
  lcd.backlight();

  // Tạo boot_id ngẫu nhiên mỗi lần khởi động lại ESP32
  randomSeed(esp_random());
  boot_id = random(100000, 999999);

  lcd.setCursor(0, 0);
  lcd.print("Ket noi Wi-Fi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi Connected! IP: " + WiFi.localIP().toString());

  // Đồng bộ thời gian NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  lcd.clear();
}

void loop() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  long rssi = WiFi.RSSI();
  String utcTime = getUTCTimeString();
  seq++;

  // Hiển thị lên LCD
  lcd.setCursor(0, 0);
  if (isnan(h) || isnan(t)) {
    lcd.print("Loi doc DHT11!  ");
  } else {
    lcd.print("T:"); lcd.print(t, 1); lcd.print("C H:"); lcd.print(h, 0); lcd.print("%   ");
  }
  lcd.setCursor(0, 1);
  lcd.print("RSSI:"); lcd.print(rssi); lcd.print("dBm    ");

  // In gói tin chuẩn bị gửi MQTT ra Serial để kiểm tra
  Serial.printf("{\"boot_id\":%u,\"seq\":%u,\"utc\":\"%s\",\"temp\":%.1f,\"hum\":%.1f,\"rssi\":%ld}\n",
                boot_id, seq, utcTime.c_str(), t, h, rssi);

  delay(1000); // Chu kỳ 1 giây theo yêu cầu Tuần 3
}