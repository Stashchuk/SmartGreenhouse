#pragma once
#include <Arduino.h>

void setupMQTT();
void updateMQTTLoop();

void publishSensorData();
void publishAppState();
