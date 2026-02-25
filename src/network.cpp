#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <time.h>
#include "config.h"
#include "network.h"
#include "sensors.h" // Потрібно для звіту (дані датчиків)
#include "logic.h"   // Потрібно для звіту (стани зон)

// 👇 РОЗДІЛЯЄМО КЛІЄНТІВ (Щоб не було конфлікту SSL)
WiFiClientSecure client;      // Цей залишаємо для MQTT (Railway)
WiFiClientSecure botClient;   // 🆕 Цей створюємо окремо для Telegram

// Прив'язуємо бота до ЙОГО власного клієнта
UniversalTelegramBot bot(BOTtoken, botClient); 

unsigned long totalOfflineMillis = 0;
unsigned long lastWifiCheckTime = 0;

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) { delay(500); attempts++; }

  if(WiFi.status() == WL_CONNECTED){
    configTzTime("EET-2EEST,M3.5.0/3,M10.5.0/4", "pool.ntp.org");
    
    // 👇 Налаштовуємо обох клієнтів (ігноруємо сертифікати для економії пам'яті)
    client.setInsecure();    // Для MQTT
    botClient.setInsecure(); // 🆕 Для Telegram
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

// Функція відправки в конкретну тему (для групи)
void sendTelegramMessage(int topicID, String text) {
  if (WiFi.status() != WL_CONNECTED) return;
  
  String postData = "{\"chat_id\": \"" + String(CHAT_ID) + "\", \"text\": \"" + text + "\", \"parse_mode\": \"HTML\", \"message_thread_id\": " + String(topicID) + "}";
  
  // 👇 ТУТ ТЕЖ ВИКОРИСТОВУЄМО botClient ЗАМІСТЬ client
  if (botClient.connect("api.telegram.org", 443)) {
    botClient.println("POST /bot" + String(BOTtoken) + "/sendMessage HTTP/1.1");
    botClient.println("Host: api.telegram.org");
    botClient.println("Content-Type: application/json");
    botClient.print("Content-Length: "); botClient.println(postData.length());
    botClient.println(); botClient.println(postData);
    
    unsigned long t = millis();
    while (botClient.available() == 0 && millis() - t < 2000);
    botClient.stop();
  }
}

// 🛠 НОВА ФУНКЦІЯ: Тільки генерує текст звіту (щоб можна було слати і в групу, і в приват)
String getReportBody(bool isStartup) {
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
  
  return msg;
}

// Ця функція викликається таймером -> шле в ГРУПУ
void sendReport(bool isStartup) {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect(); WiFi.reconnect(); return;
  }

  // Генеруємо текст
  String msg = getReportBody(isStartup);

  // Шлемо в групу (TOPIC_MAIN)
  sendTelegramMessage(TOPIC_MAIN, msg);
}

// ==========================================
// 🤖 ЛОГІКА TELEGRAM БОТА
// ==========================================

// Перевірка, чи є користувач у списку адмінів
bool isAdmin(String id) {
  for (int i = 0; i < numAdmins; i++) {
    if (String(adminChatIds[i]) == id) return true;
  }
  return false;
}

// Обробка вхідних повідомлень
void handleMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;
    String type = bot.messages[i].type;  // Отримуємо тип повідомлення
    
    // Перевіряємо, чи це натискання кнопки
    String query_data = "";
    if (type == "callback_query") {
      query_data = text; // У цій бібліотеці дані кнопки лежать тут
    }

    // 🔒 ЗАХИСТ: Якщо пише не адмін — ігноруємо
    if (!isAdmin(chat_id)) {
      bot.sendMessage(chat_id, "⛔ Немає доступу.", "");
      continue;
    }

    // 1️⃣ ОБРОБКА КНОПОК
    if (query_data != "") {
       if (query_data == "status") {
         // 👇 ТУТ ЗМІНА: Формуємо звіт і шлемо ОСОБИСТО тому, хто натиснув
         String report = getReportBody(false);
         bot.sendMessage(chat_id, report, "HTML");
         
         bot.answerCallbackQuery(bot.messages[i].query_id, "Звіт готовий!");
       }
       else if (query_data == "stop_all") {
         stopAllZones();
         bot.sendMessage(chat_id, "🛑 <b>ВСІ ЗОНИ ЗУПИНЕНО!</b>", "HTML");
       }
       else if (query_data.startsWith("on_")) {
         int z = query_data.substring(3).toInt();
         forceZoneStart(z);
         bot.sendMessage(chat_id, "▶️ Запуск: <b>" + zones[z].name + "</b>", "HTML");
       }
       else if (query_data.startsWith("off_")) {
         int z = query_data.substring(4).toInt();
         forceZoneStop(z);
         bot.sendMessage(chat_id, "⏹ Зупинка: <b>" + zones[z].name + "</b>", "HTML");
       }
       continue; 
    }

    // 2️⃣ ГОЛОВНЕ МЕНЮ (/start)
    if (text == "/start" || text == "/menu" || text == "menu") {
      String keyboardJson = "[";
      
      for (int i = 0; i < NUM_ZONES; i++) {
        keyboardJson += "[{ \"text\": \"▶️ " + zones[i].name + "\", \"callback_data\": \"on_" + String(i) + "\" },";
        keyboardJson += "{ \"text\": \"⏹ Стоп\", \"callback_data\": \"off_" + String(i) + "\" }],"; 
      }

      keyboardJson += "[{ \"text\": \"📊 СТАТУС\", \"callback_data\": \"status\" }],";
      keyboardJson += "[{ \"text\": \"🚨 СТОП ВСЕ\", \"callback_data\": \"stop_all\" }]";
      keyboardJson += "]";
      
      bot.sendMessageWithInlineKeyboard(chat_id, "🎛 <b>Меню керування:</b>", "HTML", keyboardJson);
    }
  }
}

// Головна функція
unsigned long lastBotRun = 0;
void handleTelegram() {
  if (millis() - lastBotRun > 5000) { 
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      handleMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastBotRun = millis();
  }
}