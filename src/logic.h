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
String getZoneStatusText(int i, int currentMoisture); // Для звіту