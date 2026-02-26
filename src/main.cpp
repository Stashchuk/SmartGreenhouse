#include <Arduino.h>
#include <WiFi.h>
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
      sendTelegramMessage(TOPIC_SERVICE, "✅ <b>Система запущена!</b>\nWiFi: OK");
      lcdPrintStatus("WiFi OK!");
  } else {
      lcdPrintStatus("WiFi ERROR");
  }
}

void loop() {
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