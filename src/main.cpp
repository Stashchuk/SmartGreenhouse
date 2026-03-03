#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h> // 👇 НОВЕ: Бібліотека для прошивки по Wi-Fi
#include "config.h"
#include "network.h"
#include "sensors.h"
#include "logic.h"
#include "mqtt_logic.h" 

unsigned long lastTelegramTime = 0;
bool startupReportSent = false;

void setup() {
  Serial.begin(115200);
  
  // 👇 НОВЕ: Ініціалізація пінів датчика рівня води (AJ-SR04M)
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  loadSettings(); 
  saveSettings(); 
  
  pinMode(LED_PIN, OUTPUT);
  
  setupSensors();
  lcdPrintStatus("WiFi Connect...");
  
  setupLogic();
  setupWiFi();
  setupMQTT(); 

  if (WiFi.status() == WL_CONNECTED) {
      sendTelegramMessage(TOPIC_SERVICE, "✅ <b>Система запущена по Wi-Fi !</b>\nWiFi: OK");
      lcdPrintStatus("WiFi OK!");

      // 👇 НОВЕ: Налаштування та запуск OTA (оновлення по повітрю)
      ArduinoOTA.setHostname("SmartGarden-ESP32"); // Так плата буде називатися в мережі
      ArduinoOTA.onStart([]() { Serial.println("OTA Start"); });
      ArduinoOTA.onEnd([]() { Serial.println("\nOTA End"); });
      ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
      });
      ArduinoOTA.onError([](ota_error_t error) { Serial.printf("OTA Error[%u]\n", error); });
      ArduinoOTA.begin();
      Serial.println("OTA Ready");
      // 👆 КІНЕЦЬ НОВОГО БЛОКУ

  } else {
      lcdPrintStatus("WiFi ERROR");
  }
}

void loop() {
  ArduinoOTA.handle(); // 👇 НОВЕ: Слухаємо, чи не летить нова прошивка

  checkWiFiConnection();
  handleTelegram(); 
  updateWateringLogic();
  updateSensorsLoop();
  updateLcdLoop();
  updateMQTTLoop(); 

  // 🔄 Змінено: використовуємо змінну reportInterval замість константи
  if (millis() - lastTelegramTime > reportInterval) {
    lastTelegramTime = millis();
    sendReport(false); 
  }
  
  // 🔄 Змінено: використовуємо змінну startupReportDelay замість константи
  if (!startupReportSent && millis() > startupReportDelay) {
    sendReport(true); 
    startupReportSent = true; 
  }
}