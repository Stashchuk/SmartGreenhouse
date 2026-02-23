#pragma once
#include <Arduino.h>

void setupWiFi();
void checkWiFiConnection();
String getWifiQuality();
String getUptime();
String getFormattedTime();
void sendTelegramMessage(int topicID, String text);
void sendReport(bool isStartup);
unsigned long getTotalOfflineTime(); // Функція для отримання часу офлайну