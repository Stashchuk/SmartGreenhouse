#pragma once
#include <Arduino.h>

// ==========================================
// ⚙️ НАЛАШТУВАННЯ MEREЖІ
// ==========================================
extern const char* ssid;
extern const char* password;
extern const char* BOTtoken;
extern const char* CHAT_ID; 

#define MAX_ADMINS 5            
extern const char* adminChatIds[MAX_ADMINS]; 
extern const int numAdmins;     

#define TOPIC_MAIN    10   
#define TOPIC_SERVICE 12   

// ==========================================
// ⚙️ НАЛАШТУВАННЯ ОБЛАДНАННЯ
// ==========================================
#define NUM_ZONES       4      
#define PUMP_PIN        23     
#define LED_PIN         2      

// 👇 НОВЕ: Налаштування Ультразвукового датчика (AJ-SR04M)
#define TRIG_PIN 25        // Пін для посилу сигналу
#define ECHO_PIN 26        // Пін для прийому відлуння

// 👇 НОВЕ: Калібрування бака (в сантиметрах)
#define TANK_HEIGHT  100   // Відстань від датчика до дна (0%)
#define FULL_DIST    10    // Відстань від датчика до води, коли повно (100%)
#define MIN_WATER_LEVEL 10 // Критичний рівень води у %, нижче якого насос не вмикається

// 👇 НОВЕ: Адреса датчика BME280 (зазвичай 0x76 або 0x77)
#define BME_INNER_ADDR 0x76 

// --- ⏱ ТАЙМІНГИ ---
#define VALVE_OPEN_DELAY 5000      
#define VALVE_CLOSE_DELAY 2000     
#define QUEUE_DELAY      5000      
#define SENSOR_VERIFY_TIME 30000   
#define STARTUP_REPORT_DELAY 60000 
#define REPORT_INTERVAL     3600000  
#define SAMPLE_INTERVAL 1000       
#define LCD_INTERVAL    3000       // Змінив на 3000, щоб екрани не мигтіли занадто швидко (було 20000 - це задовго)

struct Zone {
  char name[30];         
  int sensorPin;         
  int relayPin;          
  int min;               
  int max;               
  int dryVal;            
  int wetVal;            
  unsigned long wateringTime; 
  unsigned long soakingTime;  
  int topicID;           
  char lastWaterTime[6]; 
  
  // 👇 НОВЕ: Чи активна зона? (true = працює, false = вимкнена)
  bool enabled; 
};

extern Zone zones[NUM_ZONES];

void saveSettings();
void loadSettings();

enum SystemMode { MODE_AUTO, MODE_MANUAL };
extern SystemMode currentSystemMode;

bool isPumpActive(); 
// 👇 НОВІ ПРОТОТИПИ ФУНКЦІЙ
bool isWaterAvailable();
int getWaterLevelPercent(); 
void setZoneEnable(int i, bool state); // Функція вмикання/вимикання зони