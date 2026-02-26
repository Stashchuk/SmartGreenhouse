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

// 👇 НОВЕ: Функція для рухомого рядка (marquee)
String getMarqueeText(String text, int width) {
  if (text.length() <= width) return text;
  // Додаємо пробіли між повторами для чіткості
  String displayString = text + "   "; 
  int offset = (millis() / 400) % displayString.length(); // Швидкість зсуву
  String result = displayString.substring(offset) + displayString.substring(0, offset);
  return result.substring(0, width);
}

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

// 🛠 ОНОВЛЕНА ФУНКЦІЯ LCD: Додано рухомий рядок та нові цикли
void updateLcdLoop() {
  static int screen = 0; // 👈 ВИПРАВЛЕНО: Винесено на початок функції, щоб змінна була доступна скрізь
  
  if (millis() - lastLcdTime > LCD_INTERVAL) {
    lastLcdTime = millis();
    lcd.clear();
    
    char modeChar = (currentSystemMode == MODE_AUTO) ? 'A' : 'M';
    String pumpMark = isPumpActive() ? "P" : " "; 

    if (screen < NUM_ZONES) { 
      // --- ЕКРАНИ ЗОН (Цикли 0-3) ---
      int i = screen;
      ZoneState state = getZoneState(i);
      
      // Рядок 1: Z1:Назва [Режим]
      lcd.setCursor(0, 0);
      String nameMarquee = getMarqueeText(String(zones[i].name), 11);
      lcd.print("Z" + String(i + 1) + ":" + nameMarquee);
      lcd.setCursor(13, 0);
      lcd.print("[" + String(modeChar) + "]");
      
      // Рядок 2: M:45% V:OPEN [P]
      lcd.setCursor(0, 1);
      String valveStatus = (digitalRead(zones[i].relayPin) == LOW) ? "OPEN" : "CLOS";
      lcd.print("M:" + String(getPercent(i)) + "% V:" + valveStatus);
      if (isPumpActive()) {
        lcd.setCursor(14, 1);
        lcd.print("[P]");
      }
    } 
    else if (screen == 4) { 
      // --- ЕКРАН КЛІМАТУ ---
      float t, h, p;
      getAverageClimate(t, h, p);
      lcd.setCursor(0, 0);
      lcd.print("T:" + String(t, 1) + "C H:" + String(h, 0) + "%");
      lcd.setCursor(0, 1);
      lcd.print("P:" + String((int)p) + "mmHg   [" + String(modeChar) + "]");
    } 
    else if (screen == 5) {
      // --- ЕКРАН ОСТАННЬОГО ПОЛИВУ ---
      lcd.setCursor(0, 0);
      lcd.print("LST: " + String(zones[0].lastWaterTime) + " | " + String(zones[1].lastWaterTime));
      lcd.setCursor(0, 1);
      lcd.print("     " + String(zones[2].lastWaterTime) + " | " + String(zones[3].lastWaterTime));
    }

    screen = (screen + 1) % 6; 
  } else {
    // 👇 Плавний рух тексту між великими оновленнями екрана
    if (screen < NUM_ZONES) {
      int i = screen;
      if (String(zones[i].name).length() > 11) {
        lcd.setCursor(3, 0); // Позиція початку назви після "Z1:"
        lcd.print(getMarqueeText(String(zones[i].name), 11));
      }
    }
  }
}