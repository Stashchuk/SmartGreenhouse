#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_BME280.h>
#include <WiFi.h> 
#include "config.h"
#include "sensors.h"
#include "logic.h" 
#include "network.h"

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

// 👇 НОВЕ: Глобальні змінні для режиму калібрування
bool isCalibratingActive = false;
bool calibZones[NUM_ZONES] = {false, false, false, false};
unsigned long calibSum[NUM_ZONES] = {0, 0, 0, 0};
unsigned int calibCount = 0;
unsigned long calibDurationMs = 0;
unsigned long calibStartTime = 0;
unsigned long lastCalibTick = 0;
// 👆 КІНЕЦЬ НОВОГО БЛОКУ

// 👇 Функція для рухомого рядка
String getMarqueeText(String text, int width) {
  if (text.length() <= width) return text;
  String displayString = text + "   "; 
  int offset = (millis() / 350) % displayString.length(); 
  String result = displayString.substring(offset) + displayString.substring(0, offset);
  return result.substring(0, width);
}

// 👇 НОВЕ: Функція запуску калібрування (викликається з Телеграму)
void startCalibrationRoutine(bool* selected, int minutes) {
  isCalibratingActive = true;
  calibDurationMs = minutes * 60000UL; // Переводимо хвилини в мілісекунди
  calibStartTime = millis();
  lastCalibTick = millis();
  calibCount = 0;
  
  for(int i=0; i<NUM_ZONES; i++) {
    calibZones[i] = selected[i];
    calibSum[i] = 0; // Скидаємо суматори
  }
}

// Функція для перевірки, чи зона зараз на калібруванні
bool isZoneCalibrating(int zoneIndex) {
  if (!isCalibratingActive) return false;
  return calibZones[zoneIndex];
}
// 👆 КІНЕЦЬ НОВОГО БЛОКУ


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
  // 👇 НОВЕ: Логіка збору даних для калібрування (1 раз на секунду)
  if (isCalibratingActive) {
    if (millis() - lastCalibTick >= 1000) {
      lastCalibTick = millis();
      for (int i = 0; i < NUM_ZONES; i++) {
        if (calibZones[i]) {
          // Читаємо СИРЕ значення (RAW), бо саме воно потрібне для калібрування
          calibSum[i] += analogRead(zones[i].sensorPin); 
        }
      }
      calibCount++;

      // Перевіряємо, чи закінчився час тесту
      if (millis() - calibStartTime >= calibDurationMs) {
        isCalibratingActive = false; // Вимикаємо режим калібрування
        
        // Формуємо красивий звіт для Телеграму
        String report = "✅ <b>Калібрування завершено!</b>\n\n";
        for (int i = 0; i < NUM_ZONES; i++) {
          if (calibZones[i] && calibCount > 0) {
            unsigned long avgRaw = calibSum[i] / calibCount;
            report += "🌱 Зона " + String(i + 1) + " (" + String(zones[i].name) + "):\n";
            report += "Середнє RAW значення: <b>" + String(avgRaw) + "</b>\n\n";
          }
        }
        report += "<i>Використовуйте ці значення для налаштування dryVal (сухо) або wetVal (вода).</i>";
        sendTelegramMessage(TOPIC_SERVICE, report);
      }
    }
  }
  // 👆 КІНЕЦЬ НОВОГО БЛОКУ

  if (millis() - lastSampleTime > sampleInterval) { // 🔄 Змінено
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

// 🛠 ОНОВЛЕНА ФУНКЦІЯ LCD
void updateLcdLoop() {
  static int screen = 0; 
  static unsigned long lastMarqueeUpdate = 0; 
  
  if (millis() - lastLcdTime > lcdInterval) { // 🔄 Змінено
    lastLcdTime = millis();
    lcd.clear();
    
    char modeChar = (currentSystemMode == MODE_AUTO) ? 'A' : 'M';
    
    // --- ЕКРАНИ ЗОН (0, 1, 2, 3) ---
    if (screen < NUM_ZONES) { 
      int i = screen;
      
      lcd.setCursor(0, 0);
      lcd.print("Z" + String(i + 1) + ":"); 
      lcd.setCursor(13, 0);
      lcd.print("[" + String(modeChar) + "]");
      
      // 👇 ЗМІНА ТУТ: Перевіряємо, чи увімкнена зона
      if (!zones[i].enabled) {
         // ЯКЩО ВИМКНЕНА
         lcd.setCursor(0, 1);
         lcd.print("  ZONE DISABLED "); 
      } else {
         // ЯКЩО УВІМКНЕНА (Твій старий код)
         lcd.setCursor(0, 1);
         lcd.print("M:" + String(getPercent(i)) + "% ");
         
         String vStatus = (digitalRead(zones[i].relayPin) == LOW) ? "ON " : "OFF";
         lcd.print("V:" + vStatus);
         
         String pStatus = isPumpActive() ? "ON" : "OF";
         lcd.print("P:" + pStatus);
      }
    } 
    // --- ЕКРАН КЛІМАТУ (4) ---
    else if (screen == 4) { 
      float t, h, p;
      getAverageClimate(t, h, p);
      lcd.setCursor(0, 0);
      lcd.print("Temp: " + String(t, 1) + "C");
      lcd.setCursor(0, 1);
      lcd.print("Hum: " + String(h, 0) + "% P:" + String((int)p));
    } 
    // --- ЕКРАН ЧАСУ (5) ---
    else if (screen == 5) {
      lcd.setCursor(0, 0);
      lcd.print("1:" + String(zones[0].lastWaterTime) + " 2:" + String(zones[1].lastWaterTime));
      lcd.setCursor(0, 1);
      lcd.print("3:" + String(zones[2].lastWaterTime) + " 4:" + String(zones[3].lastWaterTime));
    }
    // --- ЕКРАН WIFI (6) ---
    else if (screen == 6) {
      char buf[17]; 

      int rssi = WiFi.RSSI();
      int qual = 0;
      if (rssi <= -100) qual = 0;
      else if (rssi >= -50) qual = 100;
      else qual = 2 * (rssi + 100);
      
      lcd.setCursor(0, 0);
      snprintf(buf, 17, "WiFi:%d%% %ddB", qual, rssi);
      lcd.print(buf);

      unsigned long ms = millis();
      unsigned long days = ms / 86400000;
      unsigned long hours = (ms % 86400000) / 3600000;
      unsigned long mins = (ms % 3600000) / 60000;
      
      lcd.setCursor(0, 1);
      snprintf(buf, 17, "Up: %lud %02luh %02lum", days, hours, mins);
      lcd.print(buf);
    }

    screen = (screen + 1) % 7; 
  } 

  // Оновлення рухомого рядка
  if (millis() - lastMarqueeUpdate > 300) {
    lastMarqueeUpdate = millis();
    int currentDisplayedZone = (screen == 0) ? 6 : screen - 1;

    if (currentDisplayedZone < NUM_ZONES) {
        lcd.setCursor(3, 0); 
        lcd.print(getMarqueeText(String(zones[currentDisplayedZone].name), 9));
    }
  }
}