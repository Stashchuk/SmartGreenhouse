#pragma once
#include <Arduino.h>

// Оголошення станів
enum ZoneState { 
  STATE_IDLE,       
  STATE_WAITING,    
  STATE_OPENING,    
  STATE_WATERING,   
  STATE_CLOSING,    
  STATE_ANALYZING   
};

void setupLogic();
void updateWateringLogic();

ZoneState getZoneState(int i);
unsigned long getZoneTimer(int i);
unsigned long getVerifyTimer(int i);
String getZoneStatusText(int i, int currentMoisture); 

bool isZoneActive(int i);

// Telegram / ручне керування / MQTT
void forceZoneStart(int i);
void forceZoneStop(int i);
void stopAllZones();

void setFillPumpManual(bool state);
void setIrrigationPumpManual(bool state);
void openValveManual(int i);
void closeValveManual(int i);
void closeAllValvesManual();

bool isFillPumpActive();
bool isIrrigationPumpManualActive();
bool isZoneValveOpen(int i);
