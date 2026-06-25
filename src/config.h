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

// Насос поливу, який уже був у твоєму коді
#define PUMP_PIN        23     

// НОВИЙ насос набору води.
// Я поставив GPIO27 як стартовий варіант.
// Якщо фізично підключиш реле на інший пін — зміни тільки це число.
#define FILL_PUMP_PIN   27     

#define LED_PIN         2      

// Ультразвуковий датчик рівня води AJ-SR04M
#define TRIG_PIN 25        // Пін для посилу сигналу
#define ECHO_PIN 26        // Пін для прийому відлуння

// Калібрування бака, см
#define TANK_HEIGHT  100   // Відстань від датчика до дна, 0%
#define FULL_DIST    10    // Відстань від датчика до води, коли повно, 100%
#define MIN_WATER_LEVEL 10 // Критичний рівень води у %, нижче якого насос не вмикається

// BME280
#define BME_INNER_ADDR 0x76 

// MQTT топіки для нового Android-додатку
#define MQTT_APP_PREFIX "smartgreenhouse"

// --- ⏱ ТАЙМІНГИ ---
#define DEF_VALVE_OPEN_DELAY 5000      
#define DEF_VALVE_CLOSE_DELAY 2000     
#define DEF_QUEUE_DELAY      5000      
#define DEF_SENSOR_VERIFY    30000   
#define DEF_STARTUP_DELAY    60000 
#define DEF_REPORT_INTERVAL  3600000  
#define DEF_SAMPLE_INTERVAL  1000       
#define DEF_LCD_INTERVAL     3000       

extern unsigned long valveOpenDelay;
extern unsigned long valveCloseDelay;
extern unsigned long queueDelay;
extern unsigned long sensorVerifyTime;
extern unsigned long startupReportDelay;
extern unsigned long reportInterval;
extern unsigned long sampleInterval;
extern unsigned long lcdInterval;

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
  bool enabled; 
};

extern Zone zones[NUM_ZONES];

void saveSettings();
void loadSettings();

enum SystemMode { MODE_AUTO, MODE_MANUAL };
extern SystemMode currentSystemMode;

bool isPumpActive(); 
bool isFillPumpActive();
bool isIrrigationPumpManualActive();

bool isWaterAvailable();
int getWaterLevelPercent(); 

void setZoneEnable(int i, bool state);

void setFillPumpManual(bool state);
void setIrrigationPumpManual(bool state);

void openValveManual(int i);
void closeValveManual(int i);
void closeAllValvesManual();

bool isZoneValveOpen(int i);
