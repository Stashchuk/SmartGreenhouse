#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_BME280.h>
#include "config.h"
#include "sensors.h"
#include "logic.h" // Потрібно знати стани зон для відображення на LCD

LiquidCrystal_I2C lcd(0x27, 16, 2); 
TwoWire I2C_SENSOR = TwoWire(1);
Adafruit_BME280 bme; 

unsigned long sumH[NUM_ZONES] = {0, 0, 0, 0}; 
double sumTemp = 0, sumHumAir = 0, sumPress = 0; 
int sampleCount = 0; 
bool bmeStatus = false;
unsigned long lastSampleTime = 0;
unsigned long lastLcdTime = 0;
int currentZoneDisp = 0;

void setupSensors() {
  lcd.init(); lcd.backlight();
  I2C_SENSOR.begin(32, 33, 100000);
  bmeStatus = bme.begin(0x76, &I2C_SENSOR);
}

void lcdPrintStatus(String status) {
    lcd.clear();
    lcd.print(status);
}

bool isBmeWorking() { return bmeStatus; }

int getPercent(int i) {
  int raw = analogRead(zones[i].sensorPin);
  int percent = map(raw, zones[i].dryVal, zones[i].wetVal, 0, 100);
  return constrain(percent, 0, 100);
}

void updateSensorsLoop() {
  if (millis() - lastSampleTime > SAMPLE_INTERVAL) {
    lastSampleTime = millis();
    for (int i = 0; i < NUM_ZONES; i++) sumH[i] += getPercent(i);
    if (bmeStatus) {
      sumTemp += bme.readTemperature();
      sumHumAir += bme.readHumidity();
      sumPress += (bme.readPressure() / 133.322); 
    }
    sampleCount++; 
  }
}

int getAverageMoisture(int i) {
    if (sampleCount == 0) return getPercent(i);
    return sumH[i] / sampleCount;
}

void getAverageClimate(float &t, float &h, float &p) {
    if (sampleCount == 0 && bmeStatus) {
        t = bme.readTemperature();
        h = bme.readHumidity();
        p = bme.readPressure() / 133.322;
        return;
    }
    if (sampleCount > 0) {
        t = sumTemp / sampleCount;
        h = sumHumAir / sampleCount;
        p = sumPress / sampleCount;
    }
}

void resetSensorAverages() {
    for(int i=0; i<NUM_ZONES; i++) sumH[i] = 0;
    sumTemp = 0; sumHumAir = 0; sumPress = 0; sampleCount = 0;
}

void updateLcdLoop() {
  if (millis() - lastLcdTime > LCD_INTERVAL) {
    lastLcdTime = millis();
    lcd.clear();
    int i = currentZoneDisp;
    
    lcd.setCursor(0, 0); 
    lcd.print("Z" + String(i + 1) + ":" + zones[i].name.substring(0, 12));
    
    lcd.setCursor(0, 1);
    
    ZoneState state = getZoneState(i); // Функція з logic.h
    unsigned long timer = getZoneTimer(i); // Функція з logic.h
    unsigned long currentMillis = millis();
    
    if (state == STATE_OPENING) lcd.print(">> OPENING... ");
    else if (state == STATE_WAITING) lcd.print(">> QUEUE... ");
    else if (state == STATE_WATERING) {
       long rem = (zones[i].wateringTime - (currentMillis - timer)) / 60000;
       lcd.print(">> WATER " + String(rem + 1) + "m   ");
    }
    else if (state == STATE_CLOSING) lcd.print(">> CLOSING... ");
    else if (state == STATE_ANALYZING) {
       long rem = (zones[i].soakingTime - (currentMillis - timer)) / 60000;
       lcd.print(">> WAIT " + String(rem + 1) + "m    ");
    }
    else if (getVerifyTimer(i) > 0) lcd.print(">> CHECKING... ");
    else lcd.print("M:" + String(getPercent(i)) + "% ");

    if(bmeStatus && state == STATE_IDLE && getVerifyTimer(i) == 0) 
       lcd.print("T:" + String((int)bme.readTemperature()));
       
    currentZoneDisp = (currentZoneDisp + 1) % NUM_ZONES;
  }
}