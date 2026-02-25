#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "network.h"
#include "sensors.h"
#include "logic.h"
#include "mqtt_logic.h" // <--- 1. ДОДАЛИ

unsigned long lastTelegramTime = 0;
bool startupReportSent = false;

void setup() {
  Serial.begin(115200);
  loadSettings(); // <--- 🆕 ДОДАЙ ЦЕЙ РЯДОК (Завантаження пам'яті)
  pinMode(LED_PIN, OUTPUT);
  
  setupSensors();
  lcdPrintStatus("WiFi Connect...");
  
  setupLogic();
  setupWiFi();
  setupMQTT(); // <--- 2. ЗАПУСТИЛИ MQTT

  if (WiFi.status() == WL_CONNECTED) {
      sendTelegramMessage(TOPIC_SERVICE, "✅ <b>Система запущена!</b>\nWiFi: OK");
      lcdPrintStatus("WiFi OK!");
  } else {
      lcdPrintStatus("WiFi ERROR");
  }
}

void loop() {
  checkWiFiConnection();
  handleTelegram(); // <--- 🆕 ДОДАЙ ЦЕ (Слухаємо команди)
  updateWateringLogic();
  updateSensorsLoop();
  updateLcdLoop();
  updateMQTTLoop(); // <--- 3. КРУТИМО ЦИКЛ MQTT

  if (millis() - lastTelegramTime > REPORT_INTERVAL) {
    lastTelegramTime = millis();
    sendReport(false); 
  }
  
  if (!startupReportSent && millis() > STARTUP_REPORT_DELAY) {
    sendReport(true); 
    startupReportSent = true; 
  }
}