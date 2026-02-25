#include "config.h"

// ==========================================
// 🔐 ТУТ ЖИВУТЬ ПАРОЛІ ТА НАЛАШТУВАННЯ
// ==========================================
const char* ssid      = "Stashchuk";                                      
const char* password  = "Jeka-8802";                                      
const char* BOTtoken  = "8581633125:AAECXGuCUpU7WdC2QQH5VeXxKfykXf1yX-w"; 
const char* CHAT_ID   = "-1003759921586";

// Ініціалізація масиву зон
Zone zones[NUM_ZONES] = {
  // Name           Sens Relay Min Max  Dry   Wet     Water  Soak    Topic
  {"Hamedoreya",    34,  16,   40, 80,  3391, 1073,   60000, 60000,  5}, 
  {"Happiness 1",   35,  17,   40, 60,  3377, 1065,   60000, 60000,  6}, 
  {"Happiness 2",   36,  18,   60, 80,  3380, 1057,   60000, 60000,  7}, 
  {"Happiness 3",   39,  19,   40, 60,  3405, 1133,   60000, 60000,  8}  
};