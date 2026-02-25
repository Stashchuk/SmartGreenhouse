#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <time.h>
#include "config.h"
#include "network.h"
#include "sensors.h" // Потрібно для звіту (дані датчиків)
#include "logic.h"   // Потрібно для звіту (стани зон)

// 👇 ДОДАНО: Змінні для керування діалогом налаштувань
enum BotState { IDLE, SET_NAME, SET_MIN, SET_MAX };
BotState currentState = IDLE;
int targetZone = -1; 

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

// 🛠 НОВА ФУНКЦІЯ: Тільки генерує текст звіту
String getReportBody(bool isStartup) {
  String msg = "🗓 " + getFormattedTime() + "\n";
  if (isStartup) msg += "🚀 <b>СТАРТОВИЙ АНАЛІЗ</b>\n\n";
  else msg += "📊 <b>ЗВІТ (Середнє за період)</b>\n\n";
  
  msg += "🌱 <b>Вологість Ґрунту:</b>\n";
  for (int i = 0; i < NUM_ZONES; i++) {
    int avg = getAverageMoisture(i);
    msg += "Z" + String(i+1) + " " + String(zones[i].name) + ": <b>" + String(avg) + "%</b>";
    msg += " | " + String(zones[i].min) + "-" + String(zones[i].max) + "%";
    msg += " " + getZoneStatusText(i, avg);
    msg += "\n";
  }
  resetSensorAverages(); 

  float t, h, p;
  getAverageClimate(t, h, p);
  
  msg += "\n📦 <b>Клімат:</b>\n";
  if (isBmeWorking()) {
    msg += "🌡 Т: <b>" + String(t, 1) + "°C</b>  💧 В: <b>" + String(h, 0) + "%</b>\n";
    msg += "⚙️ Тиск: <b>" + String((int)p) + " мм</b>\n";
  } else {
    msg += "⚠️ BME280 Error!\n";
  }

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

void sendReport(bool isStartup) {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect(); WiFi.reconnect(); return;
  }
  String msg = getReportBody(isStartup);
  sendTelegramMessage(TOPIC_MAIN, msg);
}

bool isAdmin(String id) {
  for (int i = 0; i < numAdmins; i++) {
    if (String(adminChatIds[i]) == id) return true;
  }
  return false;
}

// 👇 ОНОВЛЕНА ФУНКЦІЯ ДЛЯ ГОЛОВНОГО МЕНЮ (Приховує кнопки в Авто)
void gotoMainMenu(String chat_id) {
  currentState = IDLE;
  targetZone = -1;
  
  String keyboardJson = "[";
  
  // Кнопки поливу показуємо ТІЛЬКИ в ручному режимі
  if (currentSystemMode == MODE_MANUAL) {
    for (int i = 0; i < NUM_ZONES; i++) {
      keyboardJson += "[{ \"text\": \"▶️ " + String(zones[i].name) + "\", \"callback_data\": \"on_" + String(i) + "\" },";
      keyboardJson += "{ \"text\": \"⏹\", \"callback_data\": \"off_" + String(i) + "\" }],"; 
    }
  }

  keyboardJson += "[{ \"text\": \"📊 СТАТУС\", \"callback_data\": \"status\" },";
  keyboardJson += "{ \"text\": \"⚙️ НАЛАШТУВАННЯ\", \"callback_data\": \"settings_main\" }],";
  keyboardJson += "[{ \"text\": \"🚨 СТОП ВСЕ\", \"callback_data\": \"stop_all\" }]";
  keyboardJson += "]";
  
  String modeName = (currentSystemMode == MODE_AUTO) ? "🤖 АВТОМАТИКА" : "👤 РУЧНИЙ";
  bot.sendMessageWithInlineKeyboard(chat_id, "🎛 <b>Меню (" + modeName + "):</b>", "HTML", keyboardJson);
}

// 🛠 ОНОВЛЕНА ОБРОБКА ПОВІДОМЛЕНЬ
void handleMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;
    String type = bot.messages[i].type;

    if (!isAdmin(chat_id)) {
      bot.sendMessage(chat_id, "⛔ Немає доступу.", "");
      continue;
    }

    // --- ЛОГІКА ВВЕДЕННЯ ТЕКСТУ (Діалог редагування) ---
    if (type == "message" && currentState != IDLE) {
      if (targetZone != -1) {
        if (currentState == SET_NAME) {
          text.toCharArray(zones[targetZone].name, 30);
          bot.sendMessage(chat_id, "✅ Назву змінено на: <b>" + String(zones[targetZone].name) + "</b>", "HTML");
        } 
        else if (currentState == SET_MIN) {
          int val = text.toInt();
          if (val >= 0 && val < zones[targetZone].max) {
            zones[targetZone].min = val;
            bot.sendMessage(chat_id, "✅ Поріг MIN змінено на: <b>" + String(val) + "%</b>", "HTML");
          } else bot.sendMessage(chat_id, "❌ Помилка! Значення має бути від 0 до " + String(zones[targetZone].max), "");
        }
        else if (currentState == SET_MAX) {
          int val = text.toInt();
          if (val > zones[targetZone].min && val <= 100) {
            zones[targetZone].max = val;
            bot.sendMessage(chat_id, "✅ Поріг MAX змінено на: <b>" + String(val) + "%</b>", "HTML");
          } else bot.sendMessage(chat_id, "❌ Помилка! Значення має бути від " + String(zones[targetZone].min) + " до 100", "");
        }
        
        saveSettings(); // 💾 ЗБЕРЕЖЕННЯ В ПАМ'ЯТЬ
        gotoMainMenu(chat_id);
      }
      continue; 
    }

    // --- ЛОГІКА КНОПОК ---
    String query_data = (type == "callback_query") ? text : "";

    if (query_data != "") {
       if (query_data == "status") {
         bot.sendMessage(chat_id, getReportBody(false), "HTML");
         bot.answerCallbackQuery(bot.messages[i].query_id, "");
       }
       else if (query_data == "stop_all") {
         stopAllZones();
         bot.sendMessage(chat_id, "🛑 <b>ВСІ ЗОНИ ЗУПИНЕНО!</b>", "HTML");
       }
       else if (query_data.startsWith("on_")) {
         if (currentSystemMode == MODE_MANUAL) {
            int z = query_data.substring(3).toInt();
            forceZoneStart(z);
            bot.sendMessage(chat_id, "▶️ Ручний запуск: <b>" + String(zones[z].name) + "</b>", "HTML");
         } else {
            bot.sendMessage(chat_id, "❌ Помилка! Увімкніть РУЧНИЙ РЕЖИМ для керування.", "");
         }
       }
       else if (query_data.startsWith("off_")) {
         int z = query_data.substring(4).toInt();
         forceZoneStop(z);
         bot.sendMessage(chat_id, "⏹ Зупинка: <b>" + String(zones[z].name) + "</b>", "HTML");
       }
       // МЕНЮ НАЛАШТУВАНЬ
       else if (query_data == "settings_main") {
         String keyboardJson = "[";
         // Перемикач режимів
         String modeBtnText = (currentSystemMode == MODE_AUTO) ? "🔄 Перейти в РУЧНИЙ" : "🔄 Перейти в АВТО";
         keyboardJson += "[{\"text\": \"" + modeBtnText + "\", \"callback_data\": \"toggle_mode\"}],";
         
         for (int j = 0; j < NUM_ZONES; j++) {
           keyboardJson += "[{\"text\": \"⚙️ " + String(zones[j].name) + "\", \"callback_data\": \"edit_z" + String(j) + "\"}],";
         }
         keyboardJson += "[{\"text\": \"⬅️ Назад\", \"callback_data\": \"main_menu\"}]";
         keyboardJson += "]";
         bot.sendMessageWithInlineKeyboard(chat_id, "Налаштування системи:", "HTML", keyboardJson);
       }
       else if (query_data == "toggle_mode") {
          currentSystemMode = (currentSystemMode == MODE_AUTO) ? MODE_MANUAL : MODE_AUTO;
          saveSettings();
          String modeMsg = (currentSystemMode == MODE_AUTO) ? "✅ Система переведена в АВТО режим." : "⚠️ Система в РУЧНОМУ режимі. Автополив вимкнено!";
          bot.sendMessage(chat_id, modeMsg, "");
          gotoMainMenu(chat_id);
       }
       else if (query_data.startsWith("edit_z")) {
         targetZone = query_data.substring(6).toInt();
         String keyboardJson = "["
           "[{\"text\": \"🏷 Назва\", \"callback_data\": \"set_name\"}],"
           "[{\"text\": \"📉 MIN (" + String(zones[targetZone].min) + "%)\", \"callback_data\": \"set_min\"}],"
           "[{\"text\": \"📈 MAX (" + String(zones[targetZone].max) + "%)\", \"callback_data\": \"set_max\"}],"
           "[{\"text\": \"⬅️ Назад\", \"callback_data\": \"settings_main\"}]"
           "]";
         bot.sendMessageWithInlineKeyboard(chat_id, "Редагування <b>" + String(zones[targetZone].name) + "</b>:", "HTML", keyboardJson);
       }
       else if (query_data == "set_name") { currentState = SET_NAME; bot.sendMessage(chat_id, "Введіть нову назву:", ""); }
       else if (query_data == "set_min") { currentState = SET_MIN; bot.sendMessage(chat_id, "Введіть MIN вологість:", ""); }
       else if (query_data == "set_max") { currentState = SET_MAX; bot.sendMessage(chat_id, "Введіть MAX вологість:", ""); }
       else if (query_data == "main_menu") { gotoMainMenu(chat_id); }
       
       bot.answerCallbackQuery(bot.messages[i].query_id, "");
       continue; 
    }

    if (text == "/start" || text == "/menu" || text == "menu") {
      gotoMainMenu(chat_id);
    }
  }
}

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