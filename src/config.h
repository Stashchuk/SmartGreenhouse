#pragma once
#include <Arduino.h>

// ==========================================
// ⚙️ НАЛАШТУВАННЯ MEREЖІ
// ==========================================
extern const char* ssid;
extern const char* password;
extern const char* BOTtoken;
extern const char* CHAT_ID; // ID ГРУПИ (для звітів)

// 👇 НОВЕ: Список адміністраторів (хто має право керувати)
#define MAX_ADMINS 5            // Максимальна кількість адмінів
extern const char* adminChatIds[MAX_ADMINS]; // Масив ID адмінів
extern const int numAdmins;     // Реальна кількість адмінів

// ID гілок (Topics)
#define TOPIC_MAIN    10   
#define TOPIC_SERVICE 12   

// ==========================================
// ⚙️ НАЛАШТУВАННЯ ОБЛАДНАННЯ
// ==========================================
#define NUM_ZONES       4      // Кількість зон поливу (і датчиків вологості ґрунту)
#define PUMP_PIN        23     // Пін, до якого підключено реле водяного насоса
#define LED_PIN         2      // Пін вбудованого світлодіода (блимає, коли система працює)

// --- ⏱ ТАЙМІНГИ (значення в мілісекундах: 1000 мс = 1 сек) ---

#define VALVE_OPEN_DELAY 5000      // Пауза після відкриття електроклапана перед запуском насоса
#define VALVE_CLOSE_DELAY 2000     // Пауза після вимкнення насоса перед закриттям клапана
#define QUEUE_DELAY      5000      // Час між поливом різних зон
#define SENSOR_VERIFY_TIME 30000   // Час контрольного заміру

#define STARTUP_REPORT_DELAY 60000 // Пауза при старті системи
#define REPORT_INTERVAL     3600000  // Інтервал відправки даних у Телеграм

#define SAMPLE_INTERVAL 1000       // Частота опитування датчиків
#define LCD_INTERVAL    20000       // Частота оновлення дисплея

struct Zone {
  char name[30];         // ⚠️ ЗМІНЕНО: String -> char[30] (для збереження в пам'ять)
  int sensorPin;         
  int relayPin;          
  int min;               
  int max;               
  int dryVal;            
  int wetVal;            
  unsigned long wateringTime; 
  unsigned long soakingTime;  
  int topicID;
  char lastWaterTime[6]; // 🆕 НОВЕ: час останнього поливу (наприклад "12:30")
         
};

extern Zone zones[NUM_ZONES];

// 👇 НОВІ ФУНКЦІЇ ДЛЯ ПАМ'ЯТІ
void saveSettings();
void loadSettings();

enum SystemMode { MODE_AUTO, MODE_MANUAL };
extern SystemMode currentSystemMode;
bool isPumpActive();