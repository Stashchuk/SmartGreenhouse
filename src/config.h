#pragma once
#include <Arduino.h>

// ==========================================
// ⚙️ НАЛАШТУВАННЯ MEREЖІ
// ==========================================
// 👇 ТУТ ЗМІНИ: додаємо extern і прибираємо значення
extern const char* ssid;
extern const char* password;
extern const char* BOTtoken;
extern const char* CHAT_ID;

// ID гілок (Topics)
#define TOPIC_MAIN    10   
#define TOPIC_SERVICE 12   

// ==========================================
// ⚙️ НАЛАШТУВАННЯ ОБЛАДНАННЯ
// ==========================================
// ==========================================
// ⚙️ НАЛАШТУВАННЯ ОБЛАДНАННЯ
// ==========================================
#define NUM_ZONES       4      // Кількість зон поливу (і датчиків вологості ґрунту)
#define PUMP_PIN        23     // Пін, до якого підключено реле водяного насоса
#define LED_PIN         2      // Пін вбудованого світлодіода (блимає, коли система працює)

// --- ⏱ ТАЙМІНГИ (значення в мілісекундах: 1000 мс = 1 сек) ---

#define VALVE_OPEN_DELAY 5000      // Пауза після відкриття електроклапана перед запуском насоса (щоб не було гідроудару)
#define VALVE_CLOSE_DELAY 2000     // Пауза після вимкнення насоса перед закриттям клапана (скидання залишку тиску)
#define QUEUE_DELAY      5000      // Час між поливом різних зон (якщо черга на полив кількох грядок)
#define SENSOR_VERIFY_TIME 30000   // Скільки секунд чекати перед поливом для "контрольного" заміру (захист від випадкових стрибків датчика)

#define STARTUP_REPORT_DELAY 60000 // Пауза при старті системи (чекаємо, поки ESP32 надійно з'єднається з Wi-Fi та MQTT)
#define REPORT_INTERVAL     3600000 // Інтервал відправки даних у Телеграм. 3600000 мс = 1 год 

#define SAMPLE_INTERVAL 1000       // Як часто процесор опитує датчики всередині (кожної секунди)
#define LCD_INTERVAL    3000       // Як часто оновлюється текст на дисплеї боксу
struct Zone {
  String name;           
  int sensorPin;         
  int relayPin;          
  int min;               
  int max;               
  int dryVal;            
  int wetVal;            
  unsigned long wateringTime; 
  unsigned long soakingTime;  
  int topicID;           
};

extern Zone zones[NUM_ZONES];