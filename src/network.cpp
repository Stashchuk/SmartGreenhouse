#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <time.h>
#include "config.h"
#include "network.h"
#include "sensors.h" // Потрібно для звіту (дані датчиків)
#include "logic.h"   // Потрібно для звіту (стани зон)

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

unsigned long totalOfflineMillis = 0;
unsigned long lastWifiCheckTime = 0;

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) { delay(500); attempts++; }

  if(WiFi.status() == WL_CONNECTED){
    configTzTime("EET-2EEST,M3.5.0/3,M10.5.0/4", "pool.ntp.org");
    client.setInsecure(); 
  }
}

void checkWiFiConnection() {
  unsigned long currentMillis = millis();
  if (WiFi.status() != WL_CONNECTED) {
    if (currentMillis - lastWifiCheckTime > 1000) {
       totalOfflineMillis += 1000;
       lastWifiCheckTime = currentMillis;
    }
  } else {
    lastWifiCheckTime = currentMillis;
  }
}

unsigned long getTotalOfflineTime() {
    return totalOfflineMillis;
}

String getWifiQuality() {
  long rssi = WiFi.RSSI();
  String status;
  if (rssi > -50) status = "(Відмінний 🤩)";
  else if (rssi > -60) status = "(Хороший 😊)";
  else if (rssi > -70) status = "(Нормальний 👌)";
  else if (rssi > -80) status = "(Слабкий ⚠️)";
  else status = "(Критичний 🆘)";
  return String(rssi) + " dBm " + status;
}

String getUptime() {
  unsigned long sec = millis() / 1000;
  int days = sec / 86400;
  int hours = (sec % 86400) / 3600;
  int mins = (sec % 3600) / 60;
  return String(days) + "д " + String(hours) + "г " + String(mins) + "хв";
}

String getFormattedTime() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) return "Time Error";
  char buff[50];
  strftime(buff, sizeof(buff), "%d.%m.%Y %H:%M:%S", &timeinfo);
  return String(buff);
}

void sendTelegramMessage(int topicID, String text) {
  if (WiFi.status() != WL_CONNECTED) return;
  
  String postData = "{\"chat_id\": \"" + String(CHAT_ID) + "\", \"text\": \"" + text + "\", \"parse_mode\": \"HTML\", \"message_thread_id\": " + String(topicID) + "}";
  
  if (client.connect("api.telegram.org", 443)) {
    client.println("POST /bot" + String(BOTtoken) + "/sendMessage HTTP/1.1");
    client.println("Host: api.telegram.org");
    client.println("Content-Type: application/json");
    client.print("Content-Length: "); client.println(postData.length());
    client.println(); client.println(postData);
    
    unsigned long t = millis();
    while (client.available() == 0 && millis() - t < 2000);
    client.stop();
  }
}

void sendReport(bool isStartup) {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect(); WiFi.reconnect(); return;
  }

  String msg = "🗓 " + getFormattedTime() + "\n";
  if (isStartup) msg += "🚀 <b>СТАРТОВИЙ АНАЛІЗ</b>\n\n";
  else msg += "📊 <b>ЗВІТ (Середнє за період)</b>\n\n";
  
  // Дані зон
  msg += "🌱 <b>Вологість Ґрунту:</b>\n";
  for (int i = 0; i < NUM_ZONES; i++) {
    int avg = getAverageMoisture(i);
    msg += "Z" + String(i+1) + " " + zones[i].name + ": <b>" + String(avg) + "%</b>";
    msg += " | " + String(zones[i].min) + "-" + String(zones[i].max) + "%";
    
    // Отримуємо статус текстом
    msg += " " + getZoneStatusText(i, avg);
    msg += "\n";
  }
  resetSensorAverages(); // Скидання лічильників

  // Клімат
  float t, h, p;
  getAverageClimate(t, h, p);
  
  msg += "\n📦 <b>Клімат:</b>\n";
  if (isBmeWorking()) {
    msg += "🌡 Т: <b>" + String(t, 1) + "°C</b>  💧 В: <b>" + String(h, 0) + "%</b>\n";
    msg += "⚙️ Тиск: <b>" + String((int)p) + " мм</b>\n";
  } else {
    msg += "⚠️ BME280 Error!\n";
  }

  // Система
  uint32_t totalRam = ESP.getHeapSize() / 1024;
  uint32_t freeRam = ESP.getFreeHeap() / 1024;
  uint32_t usedRam = totalRam - freeRam;
  
  msg += "\n🔧 <b>СИСТЕМА:</b>\n";
  msg += "📶 WiFi: " + getWifiQuality() + "\n";
  msg += "🧠 RAM: " + String(usedRam) + "/" + String(totalRam) + " KB (Вільно: <b>" + String(freeRam) + " KB</b>)\n";
  msg += "⏱ Аптайм: " + getUptime() + "\n";
  msg += "🚫 Офлайн: " + String(totalOfflineMillis / 1000) + " сек\n";

  sendTelegramMessage(TOPIC_MAIN, msg);
}