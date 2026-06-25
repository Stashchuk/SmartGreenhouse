#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h" 
#include "sensors.h"
#include "logic.h"
#include "mqtt_logic.h"

// -----------------------------------------------------------
// MQTT сервер Railway
// -----------------------------------------------------------
const char* mqtt_server = "trolley.proxy.rlwy.net"; 
const int mqtt_port = 56092; 

// Старі топіки для графіків залишаємо, щоб нічого не зламати
const char* topic_z1_hum = "stashchuk/gh/zone1/hum";
const char* topic_z2_hum = "stashchuk/gh/zone2/hum";
const char* topic_z3_hum = "stashchuk/gh/zone3/hum";
const char* topic_z4_hum = "stashchuk/gh/zone4/hum";

WiFiClient espClient;
PubSubClient mqtt(espClient);

unsigned long lastMqttPublish = 0;
#define MQTT_INTERVAL 5000

void publishState(const String& topic, const String& payload, bool retained = true) {
  if (!mqtt.connected()) return;
  mqtt.publish(topic.c_str(), payload.c_str(), retained);
}

String zoneStateToText(ZoneState st) {
  switch (st) {
    case STATE_IDLE: return "IDLE";
    case STATE_WAITING: return "WAITING";
    case STATE_OPENING: return "OPENING";
    case STATE_WATERING: return "WATERING";
    case STATE_CLOSING: return "CLOSING";
    case STATE_ANALYZING: return "ANALYZING";
  }
  return "UNKNOWN";
}

void publishAppState() {
  if (!mqtt.connected()) return;

  publishState("smartgreenhouse/state/online", "ONLINE");
  publishState("smartgreenhouse/state/mode", currentSystemMode == MODE_MANUAL ? "MANUAL" : "AUTO");

  publishState("smartgreenhouse/state/fill_pump", isFillPumpActive() ? "ON" : "OFF");
  publishState("smartgreenhouse/state/irrigation_pump", isPumpActive() ? "ON" : "OFF");

  int waterLevel = getWaterLevelPercent();
  publishState("smartgreenhouse/state/tank/level", String(waterLevel));

  float temp, hum, press;
  getAverageClimate(temp, hum, press);
  publishState("smartgreenhouse/state/climate/temp", String(temp, 1));
  publishState("smartgreenhouse/state/climate/humidity", String(hum, 1));
  publishState("smartgreenhouse/state/climate/pressure", String(press, 1));
  publishState("smartgreenhouse/state/climate/bme", isBmeWorking() ? "OK" : "ERROR");

  for (int i = 0; i < NUM_ZONES; i++) {
    String base = "smartgreenhouse/state/zone/" + String(i + 1);

    publishState(base + "/name", String(zones[i].name));
    publishState(base + "/moisture", String(getAverageMoisture(i)));
    publishState(base + "/valve", isZoneValveOpen(i) ? "OPEN" : "CLOSE");
    publishState(base + "/watering", getZoneState(i) == STATE_WATERING ? "ON" : "OFF");
    publishState(base + "/state", zoneStateToText(getZoneState(i)));
    publishState(base + "/enabled", zones[i].enabled ? "ON" : "OFF");
    publishState(base + "/min", String(zones[i].min));
    publishState(base + "/max", String(zones[i].max));
    publishState(base + "/last_water", String(zones[i].lastWaterTime));
  }
}

void handleModeCommand(const String& payload) {
  if (payload == "MANUAL" || payload == "manual" || payload == "1") {
    currentSystemMode = MODE_MANUAL;
    saveSettings();
  }

  if (payload == "AUTO" || payload == "auto" || payload == "0") {
    currentSystemMode = MODE_AUTO;
    setIrrigationPumpManual(false);
    saveSettings();
  }
}

void handlePayloadOnOff(const String& payload, void (*setter)(bool)) {
  if (payload == "ON" || payload == "on" || payload == "1" || payload == "START") setter(true);
  if (payload == "OFF" || payload == "off" || payload == "0" || payload == "STOP") setter(false);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String t = String(topic);
  String msg;

  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  msg.trim();

  Serial.print("MQTT CMD: ");
  Serial.print(t);
  Serial.print(" = ");
  Serial.println(msg);

  if (t == "smartgreenhouse/cmd/mode") {
    handleModeCommand(msg);
    publishAppState();
    return;
  }

  if (t == "smartgreenhouse/cmd/fill_pump") {
    handlePayloadOnOff(msg, setFillPumpManual);
    publishAppState();
    return;
  }

  if (t == "smartgreenhouse/cmd/irrigation_pump") {
    handlePayloadOnOff(msg, setIrrigationPumpManual);
    publishAppState();
    return;
  }

  if (t == "smartgreenhouse/cmd/all") {
    if (msg == "STOP" || msg == "OFF" || msg == "stop" || msg == "off") {
      closeAllValvesManual();
      setFillPumpManual(false);
      setIrrigationPumpManual(false);
    }
    publishAppState();
    return;
  }

  for (int i = 0; i < NUM_ZONES; i++) {
    String valveTopic = "smartgreenhouse/cmd/zone/" + String(i + 1) + "/valve";
    String wateringTopic = "smartgreenhouse/cmd/zone/" + String(i + 1) + "/watering";
    String enabledTopic = "smartgreenhouse/cmd/zone/" + String(i + 1) + "/enabled";

    if (t == valveTopic) {
      if (msg == "OPEN" || msg == "ON" || msg == "START" || msg == "1") openValveManual(i);
      if (msg == "CLOSE" || msg == "OFF" || msg == "STOP" || msg == "0") closeValveManual(i);
      publishAppState();
      return;
    }

    if (t == wateringTopic) {
      if (msg == "START" || msg == "ON" || msg == "OPEN" || msg == "1") openValveManual(i);
      if (msg == "STOP" || msg == "OFF" || msg == "CLOSE" || msg == "0") closeValveManual(i);
      publishAppState();
      return;
    }

    if (t == enabledTopic) {
      if (msg == "ON" || msg == "1" || msg == "ENABLE") setZoneEnable(i, true);
      if (msg == "OFF" || msg == "0" || msg == "DISABLE") setZoneEnable(i, false);
      publishAppState();
      return;
    }
  }
}

void setupMQTT() {
  mqtt.setServer(mqtt_server, mqtt_port);
  mqtt.setKeepAlive(60);
  mqtt.setCallback(mqttCallback);
  mqtt.setBufferSize(512);
}

void reconnect() {
  if (WiFi.status() != WL_CONNECTED) return;

  if (!mqtt.connected()) {
    Serial.print("Connecting to Railway MQTT...");

    String clientId = "ESP32_Greenhouse_" + String(random(0xffff), HEX);

    if (mqtt.connect(
          clientId.c_str(),
          "smartgreenhouse/state/online",
          1,
          true,
          "OFFLINE"
        )) {
      Serial.println("connected!");

      mqtt.subscribe("smartgreenhouse/cmd/#");

      publishState("smartgreenhouse/state/online", "ONLINE");
      publishAppState();
      publishSensorData();
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 2 seconds");
      delay(2000);
    }
  }
}

void publishSensorData() {
  int h1 = getAverageMoisture(0);
  int h2 = getAverageMoisture(1);
  int h3 = getAverageMoisture(2);
  int h4 = getAverageMoisture(3);

  Serial.print("MQTT sensors: ");
  Serial.print(h1); Serial.print(", ");
  Serial.print(h2); Serial.print(", ");
  Serial.print(h3); Serial.print(", ");
  Serial.println(h4);

  // Старі топіки
  mqtt.publish(topic_z1_hum, String(h1).c_str());
  mqtt.publish(topic_z2_hum, String(h2).c_str());
  mqtt.publish(topic_z3_hum, String(h3).c_str());
  mqtt.publish(topic_z4_hum, String(h4).c_str());

  // Нові топіки для Android-додатку
  publishAppState();
}

void updateMQTTLoop() {
  if (!mqtt.connected()) {
    reconnect();
  }

  mqtt.loop();

  if (millis() - lastMqttPublish > MQTT_INTERVAL) {
    lastMqttPublish = millis();
    publishSensorData();
  }
}
