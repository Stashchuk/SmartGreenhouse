#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <time.h>
#include "config.h"
#include "network.h"
#include "sensors.h" 
#include "logic.h"   

enum BotState { 
  IDLE, SET_NAME, SET_MIN, SET_MAX,
  SET_T_OPEN, SET_T_CLOSE, SET_T_QUEUE, SET_T_VERIFY,
  SET_T_REPORT, SET_T_LCD
};
BotState currentState = IDLE;
int targetZone = -1; 

WiFiClientSecure client;      
WiFiClientSecure botClient;   

UniversalTelegramBot bot(BOTtoken, botClient); 

unsigned long totalOfflineMillis = 0;
unsigned long lastWifiCheckTime = 0;
unsigned long lastBotRun = 0;

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) { delay(500); attempts++; }
  if(WiFi.status() == WL_CONNECTED){
    configTzTime("EET-2EEST,M3.5.0/3,M10.5.0/4", "pool.ntp.org");
    client.setInsecure();    
    botClient.setInsecure(); 
  }
}

void checkWiFiConnection() {
  unsigned long currentMillis = millis();
  if (WiFi.status() != WL_CONNECTED) {
    if (currentMillis - lastWifiCheckTime > 1000) {
       totalOfflineMillis += 1000;
       lastWifiCheckTime = currentMillis;
    }
  } else lastWifiCheckTime = currentMillis;
}

unsigned long getTotalOfflineTime() { return totalOfflineMillis; }

String getWifiQuality() {
  long rssi = WiFi.RSSI();
  if (rssi > -50) return String(rssi) + " dBm (Відмінний 🤩)";
  if (rssi > -60) return String(rssi) + " dBm (Хороший 😊)";
  if (rssi > -70) return String(rssi) + " dBm (Нормальний 👌)";
  return String(rssi) + " dBm (Слабкий ⚠️)";
}

String getUptime() {
  unsigned long sec = millis() / 1000;
  int d = sec / 86400;
  int h = (sec % 86400) / 3600;
  int m = (sec % 3600) / 60;
  return String(d) + "д " + String(h) + "г " + String(m) + "хв";
}

String getFormattedTime() {
  struct tm ti;
  if(!getLocalTime(&ti)) return "Time Error";
  char buf[50];
  strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M:%S", &ti);
  return String(buf);
}

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

String getReportBody(bool isStartup) {
  String msg = "🗓 " + getFormattedTime() + "\n";
  msg += isStartup ? "🚀 <b>СТАРТОВИЙ АНАЛІЗ</b>\n\n" : "📊 <b>ЗВІТ (Середнє)</b>\n\n";
  msg += "🌱 <b>Вологість:</b>\n";
  for (int i = 0; i < NUM_ZONES; i++) {
    if (!zones[i].enabled) msg += "Z" + String(i+1) + " " + String(zones[i].name) + ": ⛔ <b>ВИМКНЕНО</b>\n";
    else {
      int avg = getAverageMoisture(i);
      msg += "Z" + String(i+1) + " " + String(zones[i].name) + ": <b>" + String(avg) + "%</b> | " + String(zones[i].min) + "-" + String(zones[i].max) + "% " + getZoneStatusText(i, avg) + "\n";
    }
  }
  resetSensorAverages(); 
  float t, h, p; getAverageClimate(t, h, p);
  if (isBmeWorking()) msg += "\n📦 <b>Клімат:</b>\n🌡 Т: <b>" + String(t, 1) + "°C</b>  💧 В: <b>" + String(h, 0) + "%</b>  ⚙️ <b>" + String((int)p) + " мм</b>\n";
  msg += "\n🔧 WiFi: " + getWifiQuality() + "\n⏱ Up: " + getUptime() + "\n";
  return msg;
}

void sendReport(bool isStartup) { if (WiFi.status() == WL_CONNECTED) sendTelegramMessage(TOPIC_MAIN, getReportBody(isStartup)); }

bool isAdmin(String id) {
  for (int i = 0; i < numAdmins; i++) if (String(adminChatIds[i]) == id) return true;
  return false;
}

void gotoMainMenu(String chat_id) {
  currentState = IDLE; targetZone = -1;
  String kb = "[";
  if (currentSystemMode == MODE_MANUAL) {
    for (int i = 0; i < NUM_ZONES; i++) {
      if (zones[i].enabled) kb += "[{ \"text\": \"▶️ " + String(zones[i].name) + "\", \"callback_data\": \"on_" + String(i) + "\" },{ \"text\": \"⏹\", \"callback_data\": \"off_" + String(i) + "\" }],"; 
    }
  }
  kb += "[{ \"text\": \"📊 СТАТУС\", \"callback_data\": \"status\" },{ \"text\": \"⚙️ НАЛАШТУВАННЯ\", \"callback_data\": \"settings_main\" }],";
  kb += "[{ \"text\": \"🚨 СТОП ВСЕ\", \"callback_data\": \"stop_all\" }]]";
  String mode = (currentSystemMode == MODE_AUTO) ? "🤖 АВТОМАТИКА" : "👤 РУЧНИЙ";
  bot.sendMessageWithInlineKeyboard(chat_id, "🎛 <b>Меню (" + mode + "):</b>", "HTML", kb);
}

void handleMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;
    String type = bot.messages[i].type;
    if (!isAdmin(chat_id)) continue;

    if (type == "message" && currentState != IDLE) {
      unsigned long val = text.toInt();
      bool done = false;
      // Налаштування зони - працює ЗАВЖДИ
      if (targetZone != -1) {
        if (currentState == SET_NAME) { text.toCharArray(zones[targetZone].name, 30); done = true; }
        else if (currentState == SET_MIN) { zones[targetZone].min = (int)val; done = true; }
        else if (currentState == SET_MAX) { zones[targetZone].max = (int)val; done = true; }
      } 
      // Налаштування таймінгів - ТІЛЬКИ В АВТО
      else if (currentSystemMode == MODE_AUTO) {
        if (currentState == SET_T_OPEN) { valveOpenDelay = val; done = true; }
        else if (currentState == SET_T_CLOSE) { valveCloseDelay = val; done = true; }
        else if (currentState == SET_T_QUEUE) { queueDelay = val; done = true; }
        else if (currentState == SET_T_VERIFY) { sensorVerifyTime = val; done = true; }
        else if (currentState == SET_T_REPORT) { reportInterval = val; done = true; }
        else if (currentState == SET_T_LCD) { lcdInterval = val; done = true; }
      }

      if (done) { saveSettings(); gotoMainMenu(chat_id); }
      else { bot.sendMessage(chat_id, "⚠️ Зміна таймінгів можлива лише в АВТО!", ""); currentState = IDLE; }
      continue;
    }

    // Твої оригінальні команди (завжди активні)
    if (text == "/off1") { setZoneEnable(0, false); bot.sendMessage(chat_id, "Z1 OFF", ""); continue; }
    if (text == "/on1")  { setZoneEnable(0, true);  bot.sendMessage(chat_id, "Z1 ON", ""); continue; }
    if (text == "/off2") { setZoneEnable(1, false); bot.sendMessage(chat_id, "Z2 OFF", ""); continue; }
    if (text == "/on2")  { setZoneEnable(1, true);  bot.sendMessage(chat_id, "Z2 ON", ""); continue; }
    if (text == "/off3") { setZoneEnable(2, false); bot.sendMessage(chat_id, "Z3 OFF", ""); continue; }
    if (text == "/on3")  { setZoneEnable(2, true);  bot.sendMessage(chat_id, "Z3 ON", ""); continue; }
    if (text == "/off4") { setZoneEnable(3, false); bot.sendMessage(chat_id, "Z4 OFF", ""); continue; }
    if (text == "/on4")  { setZoneEnable(3, true);  bot.sendMessage(chat_id, "Z4 ON", ""); continue; }
    if (text == "/off_all") { for(int k=0; k<NUM_ZONES; k++) setZoneEnable(k, false); bot.sendMessage(chat_id, "ALL OFF", ""); continue; }
    if (text == "/on_all") { for(int k=0; k<NUM_ZONES; k++) setZoneEnable(k, true); bot.sendMessage(chat_id, "ALL ON", ""); continue; }
    if (text == "/status") { bot.sendMessage(chat_id, getReportBody(false), "HTML"); continue; }
    if (text == "/start" || text == "/menu") { gotoMainMenu(chat_id); continue; }

    String q = (type == "callback_query") ? text : "";
    if (q != "") {
       if (q == "status") bot.sendMessage(chat_id, getReportBody(false), "HTML");
       else if (q == "stop_all") { stopAllZones(); bot.sendMessage(chat_id, "🛑 СТОП", ""); }
       // 👇 ПУСК ЗОНИ - ТІЛЬКИ В РУЧНОМУ
       else if (q.startsWith("on_")) {
          if (currentSystemMode == MODE_MANUAL) forceZoneStart(q.substring(3).toInt());
          else bot.sendMessage(chat_id, "⚠️ Керування насосом доступне лише в РУЧНОМУ режимі!", "");
       }
       else if (q.startsWith("off_")) forceZoneStop(q.substring(4).toInt());
       
       else if (q == "settings_main") {
         String kb = "[";
         kb += "[{\"text\": \"" + String(currentSystemMode==MODE_AUTO?"🔄 В РУЧНИЙ":"🔄 В АВТО") + "\", \"callback_data\": \"toggle_mode\"}],";
         kb += "[{\"text\": \"⏱ ТАЙМІНГИ\", \"callback_data\": \"settings_time\"}],";
         for (int j = 0; j < NUM_ZONES; j++) kb += "[{\"text\": \"⚙️ " + String(zones[j].name) + "\", \"callback_data\": \"edit_z" + String(j) + "\"}],";
         kb += "[{\"text\": \"⬅️ Назад\", \"callback_data\": \"main_menu\"}]]";
         bot.sendMessageWithInlineKeyboard(chat_id, "Налаштування:", "HTML", kb);
       }
       else if (q == "toggle_mode") { currentSystemMode = (currentSystemMode == MODE_AUTO) ? MODE_MANUAL : MODE_AUTO; saveSettings(); gotoMainMenu(chat_id); }
       else if (q == "settings_time") {
         if (currentSystemMode == MODE_AUTO) {
           String kb = "[";
           kb += "[{\"text\": \"⚙️ Відкриття: " + String(valveOpenDelay) + "мс\", \"callback_data\":\"t_open\"}],";
           kb += "[{\"text\": \"⚙️ Закриття: " + String(valveCloseDelay) + "мс\", \"callback_data\":\"t_close\"}],";
           kb += "[{\"text\": \"⚙️ Черга: " + String(queueDelay) + "мс\", \"callback_data\":\"t_queue\"}],";
           kb += "[{\"text\": \"⚙️ Перевірка: " + String(sensorVerifyTime) + "мс\", \"callback_data\":\"t_verify\"}],";
           kb += "[{\"text\": \"⚙️ Звіти: " + String(reportInterval / 60000) + "хв\", \"callback_data\":\"t_report\"}],";
           kb += "[{\"text\": \"⚙️ LCD: " + String(lcdInterval) + "мс\", \"callback_data\":\"t_lcd\"}],";
           kb += "[{\"text\": \"⬅️ Назад\", \"callback_data\": \"settings_main\"}]]";
           bot.sendMessageWithInlineKeyboard(chat_id, "Таймери (АВТО):", "HTML", kb);
         } else bot.sendMessage(chat_id, "⚠️ Таймінги налаштовуються лише в АВТО!", "");
       }
       else if (q.startsWith("edit_z")) {
         targetZone = q.substring(6).toInt();
         String kb = "[";
         kb += "[{\"text\":\"🏷 Ім'я: " + String(zones[targetZone].name) + "\",\"callback_data\":\"set_name\"}],";
         kb += "[{\"text\":\"📉 MIN: " + String(zones[targetZone].min) + "%\",\"callback_data\":\"set_min\"}],";
         kb += "[{\"text\":\"📈 MAX: " + String(zones[targetZone].max) + "%\",\"callback_data\":\"set_max\"}],";
         kb += "[{\"text\":\"⬅️ Назад\",\"callback_data\":\"settings_main\"}]";
         kb += "]";
         bot.sendMessageWithInlineKeyboard(chat_id, "Зона " + String(targetZone + 1), "HTML", kb);
       }
       else if (q == "set_name") { currentState = SET_NAME; bot.sendMessage(chat_id, "Введіть назву:", ""); }
       else if (q == "set_min")  { currentState = SET_MIN;  bot.sendMessage(chat_id, "Введіть MIN (%):", ""); }
       else if (q == "set_max")  { currentState = SET_MAX;  bot.sendMessage(chat_id, "Введіть MAX (%):", ""); }
       else if (q == "t_open")   { currentState = SET_T_OPEN; bot.sendMessage(chat_id, "Час відкриття (мс):", ""); }
       else if (q == "t_close")  { currentState = SET_T_CLOSE; bot.sendMessage(chat_id, "Час закриття (мс):", ""); }
       else if (q == "t_queue")  { currentState = SET_T_QUEUE; bot.sendMessage(chat_id, "Пауза черги (мс):", ""); }
       else if (q == "t_verify") { currentState = SET_T_VERIFY; bot.sendMessage(chat_id, "Час перевірки (мс):", ""); }
       else if (q == "t_report") { currentState = SET_T_REPORT; bot.sendMessage(chat_id, "Інтервал звітів (мс):", ""); }
       else if (q == "t_lcd")    { currentState = SET_T_LCD; bot.sendMessage(chat_id, "Швидкість LCD (мс):", ""); }
       else if (q == "main_menu") gotoMainMenu(chat_id);
       bot.answerCallbackQuery(bot.messages[i].query_id, "");
    }
  }
}

void handleTelegram() {
  if (millis() - lastBotRun > 1000) { 
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) { handleMessages(numNewMessages); numNewMessages = bot.getUpdates(bot.last_message_received + 1); }
    lastBotRun = millis();
  }
}