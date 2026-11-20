#include <Arduino.h> // Bắt buộc phải có trên PlatformIO
#include <WiFi.h>
#include "time.h"

const char* ssid = "huststaff";
const char* password = "1";

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;      
const int   daylightOffset_sec = 0; 

const char* pi_host = "192.168.192.168.0.188 10.136.73.49"; 

void printUTCTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Loi: Chua lay duoc thoi gian tu NTP!");
    return;
  }
  Serial.printf("Gio UTC: %04d-%02d-%02d %02d:%02d:%02d\n",
                timeinfo.tm_year + 1900,
                timeinfo.tm_mon + 1,
                timeinfo.tm_mday,
                timeinfo.tm_hour,
                timeinfo.tm_min,
                timeinfo.tm_sec);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.print("Dang ket noi Wi-Fi: ");
  Serial.println(ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nDa ket noi Wi-Fi thanh cong!");
  Serial.print("Dia chi IP cua ESP32: ");
  Serial.println(WiFi.localIP());

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Dang dong bo thoi gian NTP...");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    long rssi = WiFi.RSSI();
    Serial.print("Cuong do tin hieu (RSSI): ");
    Serial.print(rssi);
    Serial.println(" dBm");

    printUTCTime();

    Serial.print("San sang gui du lieu toi Pi tai dia chi: ");
    Serial.println(pi_host);
    Serial.println("-----------------------------------");
  } else {
    Serial.println("Mat ket noi Wi-Fi! Dang thu ket noi lai...");
    WiFi.reconnect();
  }

  delay(5000);
}