#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h> 
#include "config.h"
#include "network.h"
#include "sensors.h"
#include "logic.h"
#include "mqtt_logic.h" 

unsigned long lastTelegramTime = 0;
unsigned long lastBotCheck = 0; // 👇 ДОДАНО: Таймер для Telegram
bool startupReportSent = false;

void setup() {
  Serial.begin(115200);
  
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
      // 👇 OTA налаштовується ТІЛЬКИ якщо є Wi-Fi
      ArduinoOTA.setHostname("SmartGarden-ESP32"); 
      ArduinoOTA.onStart([]() { Serial.println("OTA Start"); });
      ArduinoOTA.onEnd([]() { Serial.println("\nOTA End"); });
      ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
      });
      ArduinoOTA.onError([](ota_error_t error) { Serial.printf("OTA Error[%u]\n", error); });
      ArduinoOTA.begin();
      
      Serial.println("OTA Ready");
      sendTelegramMessage(TOPIC_SERVICE, "✅ <b>Система запущена!!!</b>\nOTA Ready\nWi-Fi OK with IP: " + WiFi.localIP().toString());
      lcdPrintStatus("WiFi OK!");
  } else {
      lcdPrintStatus("WiFi ERROR");
  }
}

void loop() {
  // 1. Пріоритет №1 — перевірка прошивки
  ArduinoOTA.handle(); 

  checkWiFiConnection();

  // 2. Опитуємо Telegram раз на 2 секунди, щоб не "душити" Wi-Fi
  if (millis() - lastBotCheck > 2000) {
    handleTelegram(); 
    lastBotCheck = millis();
  }

  updateWateringLogic();
  updateSensorsLoop();
  updateLcdLoop();
  updateMQTTLoop(); 

  if (millis() - lastTelegramTime > reportInterval) {
    lastTelegramTime = millis();
    sendReport(false); 
  }
  
  if (!startupReportSent && millis() > startupReportDelay) {
    sendReport(true); 
    startupReportSent = true; 
  }
}