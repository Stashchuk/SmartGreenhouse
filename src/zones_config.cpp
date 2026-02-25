#include "config.h"
#include <Preferences.h> // Бібліотека для роботи з пам'яттю

Preferences preferences; // Об'єкт для збереження даних

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
Zone zones[NUM_ZONES] = {
  // Name (max 29 chars)  Sens Relay Min Max  Dry   Wet     Water  Soak    Topic
  {"Hamedoreya",          34,  16,   40, 80,  3391, 1073,   60000, 60000,  5}, 
  {"Happiness 1",         35,  17,   40, 60,  3377, 1065,   60000, 60000,  6}, 
  {"Happiness 2",         36,  18,   40, 60,  3380, 1057,   60000, 60000,  7}, 
  {"Happiness 3",         39,  19,   40, 60,  3405, 1133,   60000, 60000,  8}  
};

// ==========================================
// 💾 ФУНКЦІЇ РОБОТИ З ПАМ'ЯТТЮ
// ==========================================

// Зберегти поточні налаштування у вічну пам'ять
void saveSettings() {
  preferences.begin("garden-data", false); // Відкриваємо комірку пам'яті (false = режим запису)
  
  // Записуємо весь масив зон одним махом
  preferences.putBytes("zones", zones, sizeof(zones));
  
  preferences.end();
  Serial.println("💾 НАЛАШТУВАННЯ ЗБЕРЕЖЕНО В ПАМ'ЯТЬ!");
}

// Завантажити налаштування при старті
void loadSettings() {
  preferences.begin("garden-data", true); // Відкриваємо (true = тільки читання)
  
  // Перевіряємо, чи є збережені дані
  if (preferences.isKey("zones")) {
    preferences.getBytes("zones", zones, sizeof(zones));
    Serial.println("📂 НАЛАШТУВАННЯ ЗАВАНТАЖЕНО З ПАМ'ЯТІ!");
  } else {
    Serial.println("⚠️ Пам'ять пуста. Використовуються заводські налаштування.");
    // Можна зберегти заводські, щоб наступного разу вони були в пам'яті
    // saveSettings(); 
  }
  
  preferences.end();
}