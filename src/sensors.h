#pragma once
#include <Arduino.h>

void setupSensors();
void updateSensorsLoop();
void updateLcdLoop();
int getPercent(int i);
int getAverageMoisture(int i);
void getAverageClimate(float &t, float &h, float &p);
void resetSensorAverages();
bool isBmeWorking();
void lcdPrintStatus(String status); // Для виводу WiFi статусу при старті

// 👇 ДОДАНО: Прототип для функції рухомого рядка (marquee)
String getMarqueeText(String text, int width);

// 👇 НОВЕ: Прототипи для калібрування (щоб їх бачив Телеграм і логіка поливу)
void startCalibrationRoutine(bool* selectedZones, int minutes);
bool isZoneCalibrating(int zoneIndex);
// 👆 КІНЕЦЬ НОВОГО БЛОКУ