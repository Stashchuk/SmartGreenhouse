#include "config.h"
#include <Preferences.h> // Бібліотека для роботи з пам'яттю

Preferences preferences; // Об'єкт для збереження даних

// 👇 НОВЕ: Змінна режиму системи (Авто за замовчуванням)
SystemMode currentSystemMode = MODE_AUTO;

// ==========================================
// 🔐 ТУТ ЖИВУТЬ ПАРОЛІ ТА НАЛАШТУВАННЯ
// ==========================================
const char* ssid      = "Stashchuk";                                      
const char* password  = "Jeka-8802";                                      
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
// ⚠️ ДОДАНО: Час останнього поливу ("--:--") та статус Enabled (true)
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
  preferences.begin("garden-v4", false); // 👈 ЗМІНЕНО НА v4 (режим запису)
  
  // Записуємо весь масив зон одним махом
  preferences.putBytes("zones", zones, sizeof(zones));
  
  // Записуємо режим системи
  preferences.putInt("sysMode", (int)currentSystemMode);
  
  preferences.end();
  Serial.println("💾 НАЛАШТУВАННЯ ТА РЕЖИМ ЗБЕРЕЖЕНО В ПАМ'ЯТЬ (v4)!");
}

// Завантажити налаштування при старті
void loadSettings() {
  preferences.begin("garden-v4", true); // 👈 ЗМІНЕНО НА v4 (тільки читання)
  
  // Перевіряємо, чи є збережені дані зон
  if (preferences.isKey("zones")) {
    preferences.getBytes("zones", zones, sizeof(zones));
    Serial.println("📂 НАЛАШТУВАННЯ ЗАВАНТАЖЕНО З ПАМ'ЯТІ (v4)!");
  } else {
    Serial.println("⚠️ Пам'ять зон порожня. Використовуються заводські налаштування.");
  }
  
  // Завантажуємо режим системи (якщо немає - MODE_AUTO)
  int savedMode = preferences.getInt("sysMode", (int)MODE_AUTO);
  currentSystemMode = (SystemMode)savedMode;
  Serial.print("📂 ПОТОЧНИЙ РЕЖИМ: ");
  Serial.println(currentSystemMode == MODE_AUTO ? "AUTO" : "MANUAL");
  
  preferences.end();
}