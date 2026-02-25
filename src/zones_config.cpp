#include "config.h"

// ==========================================
// 🔐 ТУТ ЖИВУТЬ ПАРОЛІ ТА НАЛАШТУВАННЯ
// ==========================================
const char* ssid      = "Stashchuk";                                      
const char* password  = "Jeka-8802";                                      
const char* BOTtoken  = "8581633125:AAECXGuCUpU7WdC2QQH5VeXxKfykXf1yX-w"; 
const char* CHAT_ID   = "-1003759921586"; // ID ГРУПИ (куди йдуть звіти)

// 👇 СПИСОК АДМІНІСТРАТОРІВ (Кому можна керувати)
// Впиши сюди свій ID (отримай через @userinfobot)
const char* adminChatIds[MAX_ADMINS] = {
  "7350249729",  // <--- Встав сюди свій ID (наприклад "385720485")
  "",              // Сюди можна вписати ID дружини або партнера
  "", 
  "", 
  "" 
};

// Кількість активних адмінів (якщо вписав тільки себе — став 1)
const int numAdmins = 1; 

// Ініціалізація масиву зон
Zone zones[NUM_ZONES] = {
  // Name           Sens Relay Min Max  Dry   Wet     Water  Soak    Topic
  {"Hamedoreya",    34,  16,   40, 80,  3391, 1073,   60000, 60000,  5}, 
  {"Happiness 1",   35,  17,   40, 60,  3377, 1065,   60000, 60000,  6}, 
  {"Happiness 2",   36,  18,   40, 60,  3380, 1057,   60000, 60000,  7}, 
  {"Happiness 3",   39,  19,   40, 60,  3405, 1133,   60000, 60000,  8}  
};