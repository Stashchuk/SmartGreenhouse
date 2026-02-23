#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h" 
#include "sensors.h"

// -----------------------------------------------------------
// НАЛАШТУВАННЯ СЕРВЕРА (RAILWAY)
// -----------------------------------------------------------
const char* mqtt_server = "trolley.proxy.rlwy.net"; 
const int mqtt_port = 56092; 

// Унікальні топіки
const char* topic_z1_hum = "stashchuk/gh/zone1/hum";
const char* topic_z2_hum = "stashchuk/gh/zone2/hum";
const char* topic_z3_hum = "stashchuk/gh/zone3/hum";
const char* topic_z4_hum = "stashchuk/gh/zone4/hum";

WiFiClient espClient;
PubSubClient mqtt(espClient);

unsigned long lastMqttPublish = 0;
#define MQTT_INTERVAL 5000 

void setupMQTT() {
  mqtt.setServer(mqtt_server, mqtt_port);
}

void reconnect() {
  // Якщо немає WiFi - виходимо
  if (WiFi.status() != WL_CONNECTED) return;

  // Якщо MQTT не підключено - пробуємо
  if (!mqtt.connected()) {
    Serial.print("Connecting to Railway MQTT...");
    
    String clientId = "ESP32_Greenhouse_" + String(random(0xffff), HEX);
    
    // Підключення (без логіна/пароля)
    if (mqtt.connect(clientId.c_str())) {
      Serial.println("connected! 🚀");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 2 seconds");
      delay(2000);
    }
  }
}

void publishSensorData() {
  // Отримуємо дані
  int h1 = getAverageMoisture(0);
  int h2 = getAverageMoisture(1);
  int h3 = getAverageMoisture(2);
  int h4 = getAverageMoisture(3);

  // Виводимо в монітор для перевірки
  Serial.print("Sending: ");
  Serial.print(h1); Serial.print(", ");
  Serial.print(h2); Serial.print(", ");
  Serial.print(h3); Serial.print(", ");
  Serial.println(h4);

  // Відправляємо на Railway
  mqtt.publish(topic_z1_hum, String(h1).c_str());
  mqtt.publish(topic_z2_hum, String(h2).c_str());
  mqtt.publish(topic_z3_hum, String(h3).c_str());
  mqtt.publish(topic_z4_hum, String(h4).c_str());
}

void updateMQTTLoop() {
  if (!mqtt.connected()) {
    reconnect();
  }
  mqtt.loop();

  // Таймер відправки
  if (millis() - lastMqttPublish > MQTT_INTERVAL) {
    lastMqttPublish = millis();
    publishSensorData();
  }
} 
// <--- ОСЬ ЦІЄЇ ДУЖКИ НЕ ВИСТАЧАЛО
// <--- ОСЬ ЦІЄЇ ДУЖКИ НЕ ВИСТАЧАЛО