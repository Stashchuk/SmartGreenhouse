#include "config.h"
#include <Preferences.h> // Бібліотека для роботи з пам'яттю

Preferences preferences; // Об'єкт для збереження даних

// 👇 НОВЕ: Змінна режиму системи (Авто за замовчуванням)
SystemMode currentSystemMode = MODE_AUTO;

// 👇 НОВЕ: Оголошення змінних таймінгів (ініціалізуємо дефолтними значеннями з config.h)
unsigned long valveOpenDelay     = DEF_VALVE_OPEN_DELAY;
unsigned long valveCloseDelay    = DEF_VALVE_CLOSE_DELAY;
unsigned long queueDelay         = DEF_QUEUE_DELAY;
unsigned long sensorVerifyTime   = DEF_SENSOR_VERIFY;
unsigned long startupReportDelay = DEF_STARTUP_DELAY;
unsigned long reportInterval     = DEF_REPORT_INTERVAL;
unsigned long sampleInterval     = DEF_SAMPLE_INTERVAL;
unsigned long lcdInterval        = DEF_LCD_INTERVAL;

// ==========================================
// 🔐 ТУТ ЖИВУТЬ ПАРОЛІ ТА НАЛАШТУВАННЯ
// ==========================================
const char* ssid      = "Galaxy";                                      
const char* password  = "11111111";                                      
const char* BOTtoken  = "8581633125:AAECXGuCUpU7WdC2QQH5VeXxKfykXf1yX-w"; 
const char* CHAT_ID   = "-1003759921586"; // ID ГРУПИ (куди йдуть звіти)

// 👇 СПИСОК АДМІНІСТРАТОРІВ (Кому можна керувати)
const char* adminChatIds[MAX_ADMINS] = {
  "7350249729",  // Твій ID
  "",            // Резерв
  "", 
  "", 
  "" 
};

// Кількість активних адмінів
const int numAdmins = 1; 

// Ініціалізація масиву зон (Заводські налаштування)
Zone zones[NUM_ZONES] = {
  // Name (max 29 chars)  Sens Relay Min Max  Dry   Wet     Water  Soak    Topic  LastTime Enabled
  {"Hamedoreya",          34,  16,   40, 80,  3391, 1073,   60000, 60000,  5,     "--:--", true}, 
  {"Happiness 1",         35,  17,   40, 60,  3377, 1065,   60000, 60000,  6,     "--:--", true}, 
  {"Happiness 2",         36,  18,   40, 60,  3380, 1057,   60000, 60000,  7,     "--:--", true}, 
  {"Happiness 3",         39,  19,   40, 60,  3405, 1133,   60000, 60000,  8,     "--:--", true}  
};

// ==========================================
// 💾 ФУНКЦІЇ РОБОТИ З ПАМ'ЯТТЮ
// ==========================================

// Зберегти поточні налаштування у вічну пам'ять
void saveSettings() {
  preferences.begin("garden-v4", false); 
  
  // Записуємо весь масив зон одним махом
  preferences.putBytes("zones", zones, sizeof(zones));
  
  // Записуємо режим системи
  preferences.putInt("sysMode", (int)currentSystemMode);
  
  // 👇 НОВЕ: Зберігаємо таймінги
  preferences.putULong("t_v_open", valveOpenDelay);
  preferences.putULong("t_v_close", valveCloseDelay);
  preferences.putULong("t_queue", queueDelay);
  preferences.putULong("t_verify", sensorVerifyTime);
  preferences.putULong("t_startup", startupReportDelay);
  preferences.putULong("t_report", reportInterval);
  preferences.putULong("t_sample", sampleInterval);
  preferences.putULong("t_lcd", lcdInterval);

  preferences.end();
  Serial.println("💾 НАЛАШТУВАННЯ ТА ТАЙМІНГИ ЗБЕРЕЖЕНО (v4)!");
}

// Завантажити налаштування при старті
void loadSettings() {
  preferences.begin("garden-v4", true); // Тільки читання
  
  // Перевіряємо, чи є збережені дані зон
  if (preferences.isKey("zones")) {
    preferences.getBytes("zones", zones, sizeof(zones));
    Serial.println("📂 ЗОНИ ЗАВАНТАЖЕНО З ПАМ'ЯТІ (v4)!");
  } else {
    Serial.println("⚠️ Пам'ять зон порожня. Використовуються заводські налаштування.");
  }
  
  // Завантажуємо режим системи
  int savedMode = preferences.getInt("sysMode", (int)MODE_AUTO);
  currentSystemMode = (SystemMode)savedMode;
  
  // 👇 НОВЕ: Завантажуємо таймінги (якщо немає в пам'яті - беремо дефолтні)
  valveOpenDelay     = preferences.getULong("t_v_open", DEF_VALVE_OPEN_DELAY);
  valveCloseDelay    = preferences.getULong("t_v_close", DEF_VALVE_CLOSE_DELAY);
  queueDelay         = preferences.getULong("t_queue", DEF_QUEUE_DELAY);
  sensorVerifyTime   = preferences.getULong("t_verify", DEF_SENSOR_VERIFY);
  startupReportDelay = preferences.getULong("t_startup", DEF_STARTUP_DELAY);
  reportInterval     = preferences.getULong("t_report", DEF_REPORT_INTERVAL);
  sampleInterval     = preferences.getULong("t_sample", DEF_SAMPLE_INTERVAL);
  lcdInterval        = preferences.getULong("t_lcd", DEF_LCD_INTERVAL);
  
  Serial.print("📂 ПОТОЧНИЙ РЕЖИМ: ");
  Serial.println(currentSystemMode == MODE_AUTO ? "AUTO" : "MANUAL");
  
  preferences.end();
}