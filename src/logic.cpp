#include "config.h"
#include "logic.h"
#include "sensors.h"
#include "network.h"

ZoneState zoneStates[NUM_ZONES];       
unsigned long zoneTimers[NUM_ZONES];   
unsigned long verifyTimers[NUM_ZONES]; 
unsigned long nextAllowedActionTime = 0; 

unsigned long lastManualNotify[NUM_ZONES] = {0, 0, 0, 0};

// Ручні стани для MQTT / Android-додатку
bool fillPumpManualState = false;
bool irrigationPumpManualState = false;

bool isPumpActive() {
  return (digitalRead(PUMP_PIN) == LOW); 
}

bool isFillPumpActive() {
  return (digitalRead(FILL_PUMP_PIN) == LOW);
}

bool isIrrigationPumpManualActive() {
  return irrigationPumpManualState;
}

bool isZoneValveOpen(int i) {
  if (i < 0 || i >= NUM_ZONES) return false;
  return digitalRead(zones[i].relayPin) == LOW;
}

int getWaterLevelPercent() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000UL);

  // Якщо датчик не відповідає — повертаємо -1.
  // У такому випадку насос не блокуємо, щоб система не стала через відсутній датчик.
  if (duration == 0) return -1;

  float distance = duration * 0.0343f / 2.0f;

  int level = map((int)distance, TANK_HEIGHT, FULL_DIST, 0, 100);
  return constrain(level, 0, 100);
}

bool isWaterAvailable() {
  int level = getWaterLevelPercent();

  // -1 = датчик не відповів. Поки не блокуємо роботу.
  if (level == -1) return true;

  return level >= MIN_WATER_LEVEL;
}

void setZoneEnable(int i, bool state) {
  if (i < 0 || i >= NUM_ZONES) return;

  zones[i].enabled = state;

  if (!state && zoneStates[i] != STATE_IDLE) {
    forceZoneStop(i);
    zoneStates[i] = STATE_IDLE;
  }

  saveSettings();
}

void setFillPumpManual(bool state) {
  fillPumpManualState = state;
  digitalWrite(FILL_PUMP_PIN, state ? LOW : HIGH);
  Serial.printf("MQTT: Fill pump %s\n", state ? "ON" : "OFF");
}

void setIrrigationPumpManual(bool state) {
  currentSystemMode = MODE_MANUAL;
  irrigationPumpManualState = state;
  digitalWrite(PUMP_PIN, state ? LOW : HIGH);
  saveSettings();
  Serial.printf("MQTT: Irrigation pump %s\n", state ? "ON" : "OFF");
}

void setupLogic() {
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, HIGH); 

  pinMode(FILL_PUMP_PIN, OUTPUT);
  digitalWrite(FILL_PUMP_PIN, HIGH);

  for(int i=0; i<NUM_ZONES; i++) {
    pinMode(zones[i].relayPin, OUTPUT);
    digitalWrite(zones[i].relayPin, HIGH); 
    zoneStates[i] = STATE_IDLE;
    zoneTimers[i] = 0;
    verifyTimers[i] = 0;

    if (strlen(zones[i].lastWaterTime) == 0) {
      strcpy(zones[i].lastWaterTime, "--:--");
    }
  }
}

ZoneState getZoneState(int i) { return zoneStates[i]; }
unsigned long getZoneTimer(int i) { return zoneTimers[i]; }
unsigned long getVerifyTimer(int i) { return verifyTimers[i]; }

String getZoneStatusText(int i, int currentMoisture) {
  if (!zones[i].enabled) return "⛔ (ВИМКНЕНО)";

  if (zoneStates[i] == STATE_WATERING) return "💦 (Полив)";
  if (zoneStates[i] == STATE_ANALYZING) return "⏳ (Вбирання)";
  if (zoneStates[i] == STATE_WAITING) return "🕒 (В черзі)";
  if (zoneStates[i] == STATE_OPENING) return "⚙️ (Відкриття)";
  if (zoneStates[i] == STATE_CLOSING) return "🔒 (Закриття)";
  if (zoneStates[i] == STATE_IDLE && verifyTimers[i] > 0) return "🧐 (Перевірка...)";

  if (currentMoisture < zones[i].min) return "💧 (Сухо)";
  if (currentMoisture > zones[i].max) return "🌊 (Мокро)";
  return "✅ (Норма)";
}

void checkManualAlerts() {
  if (currentSystemMode == MODE_MANUAL) {
    unsigned long currentMillis = millis();
    for (int i = 0; i < NUM_ZONES; i++) {
      if (zoneStates[i] == STATE_WATERING) {
        unsigned long activeTimeMin = (currentMillis - zoneTimers[i]) / 60000;
        if (activeTimeMin >= 30 && (activeTimeMin / 30) > (lastManualNotify[i] / 30)) {
          lastManualNotify[i] = activeTimeMin;
          sendTelegramMessage(TOPIC_MAIN, "⚠️ <b>УВАГА!</b>\nЗона <b>" + String(zones[i].name) + "</b> поливає вже " + String(activeTimeMin) + " хв!\nНе забудьте вимкнути.");
        }
      } else {
        lastManualNotify[i] = 0;
      }
    }
  }
}

void updateWateringLogic() {
  unsigned long currentMillis = millis();
  bool isPumpNeeded = false; 

  checkManualAlerts();

  if (!isWaterAvailable()) {
    if (isPumpActive()) {
      stopAllZones();
      setIrrigationPumpManual(false);
      sendTelegramMessage(TOPIC_MAIN, "🚨 <b>АВАРІЯ! НЕМАЄ ВОДИ!</b>\nНасос зупинено.");
    }
    return; 
  }

  bool isMechanismBusy = false;
  for (int j = 0; j < NUM_ZONES; j++) {
    if (zoneStates[j] == STATE_OPENING || zoneStates[j] == STATE_CLOSING) {
      isMechanismBusy = true; 
      break; 
    }
  }

  bool isQueueLocked = (currentMillis < nextAllowedActionTime);

  for (int i = 0; i < NUM_ZONES; i++) {
    if (!zones[i].enabled) {
      zoneStates[i] = STATE_IDLE; 
      continue; 
    }

    if (isZoneCalibrating(i)) {
      zoneStates[i] = STATE_IDLE; 
      continue; 
    }

    int moisture = getPercent(i);

    switch (zoneStates[i]) {
      case STATE_IDLE:
        if (currentSystemMode == MODE_AUTO && moisture < zones[i].min) {
          if (verifyTimers[i] == 0) {
            verifyTimers[i] = currentMillis; 
            Serial.printf("Z%d: Start verification. Moisture: %d%%\n", i+1, moisture);
          } 
          else if (currentMillis - verifyTimers[i] >= sensorVerifyTime) {
            verifyTimers[i] = 0; 
            zoneStates[i] = STATE_WAITING; 
            sendTelegramMessage(zones[i].topicID, "⚠️ <b>Критично сухо!</b> (" + String(moisture) + "%)\nПочинаю цикл поливу...");
          }
        } 
        else if (moisture >= zones[i].min + 5) { 
          if (verifyTimers[i] > 0) verifyTimers[i] = 0; 
        }
        break;

      case STATE_WAITING:
        if (!isMechanismBusy && !isQueueLocked) {
          digitalWrite(zones[i].relayPin, LOW); 
          zoneStates[i] = STATE_OPENING;
          zoneTimers[i] = currentMillis;
          isMechanismBusy = true;
          sendTelegramMessage(zones[i].topicID, "⚙️ <b>Відкриваю клапан...</b>");
        }
        break;

      case STATE_OPENING:
        if (currentMillis - zoneTimers[i] >= valveOpenDelay) {
          zoneStates[i] = STATE_WATERING; 
          zoneTimers[i] = currentMillis;
          nextAllowedActionTime = currentMillis + queueDelay;
          sendTelegramMessage(zones[i].topicID, "💦 <b>Полив СТАРТ!</b>\nТривалість: " + String(zones[i].wateringTime / 60000) + " хв");
        }
        break;

      case STATE_WATERING:
        isPumpNeeded = true; 
        if (currentSystemMode == MODE_AUTO && (currentMillis - zoneTimers[i] >= zones[i].wateringTime)) {
          zoneStates[i] = STATE_CLOSING; 
          zoneTimers[i] = currentMillis;
        }
        break;

      case STATE_CLOSING:
        if (currentMillis - zoneTimers[i] >= valveCloseDelay) {
          digitalWrite(zones[i].relayPin, HIGH); 

          struct tm timeinfo;
          if(getLocalTime(&timeinfo)) {
            strftime(zones[i].lastWaterTime, 6, "%H:%M", &timeinfo);
          } else {
            strcpy(zones[i].lastWaterTime, "??:??");
          }

          saveSettings();

          zoneStates[i] = (currentSystemMode == MODE_AUTO) ? STATE_ANALYZING : STATE_IDLE;
          zoneTimers[i] = currentMillis;
          nextAllowedActionTime = currentMillis + queueDelay;

          if (currentSystemMode == MODE_AUTO)
            sendTelegramMessage(zones[i].topicID, "✅ <b>Полив СТОП.</b>\nКлапан закрито. Чекаю вбирання...");
          else
            sendTelegramMessage(zones[i].topicID, "⏹ <b>Ручний полив завершено.</b>\nКлапан закрито.");
        }
        break;

      case STATE_ANALYZING:
        if (currentSystemMode == MODE_MANUAL) { 
          zoneStates[i] = STATE_IDLE; 
          break; 
        }

        if (currentMillis - zoneTimers[i] >= zones[i].soakingTime) {
          if (moisture < zones[i].max) {
            zoneStates[i] = STATE_WAITING; 
            sendTelegramMessage(zones[i].topicID, "💧 Вологість " + String(moisture) + "%. Треба до " + String(zones[i].max) + "%.\n🔄 Повторюю цикл!");
          } 
          else {
            zoneStates[i] = STATE_IDLE; 
            sendTelegramMessage(zones[i].topicID, "🆗 <b>Полито повністю!</b> (" + String(moisture) + "%).\nСплю.");
          }
        }
        break;
    }
  }

  if (currentSystemMode == MODE_MANUAL) {
    if (isPumpNeeded || irrigationPumpManualState) digitalWrite(PUMP_PIN, LOW);
    else digitalWrite(PUMP_PIN, HIGH);
  } else {
    irrigationPumpManualState = false;
    if (isPumpNeeded) digitalWrite(PUMP_PIN, LOW);
    else digitalWrite(PUMP_PIN, HIGH);
  }
}

bool isZoneActive(int i) {
  return (zoneStates[i] != STATE_IDLE && zoneStates[i] != STATE_ANALYZING);
}

void forceZoneStart(int i) {
  if (i < 0 || i >= NUM_ZONES) return;

  if (!zones[i].enabled) {
    sendTelegramMessage(TOPIC_MAIN, "⛔ <b>Помилка:</b> Зона " + String(i+1) + " вимкнена в налаштуваннях!");
    return;
  }

  if (isZoneCalibrating(i)) {
    sendTelegramMessage(TOPIC_MAIN, "⛔ <b>Помилка:</b> Зона " + String(i+1) + " зараз калібрується!");
    return;
  }

  if (!isWaterAvailable()) {
    sendTelegramMessage(TOPIC_MAIN, "❌ <b>Неможливо запустити: Немає води!</b>");
    return;
  }

  currentSystemMode = MODE_MANUAL;
  saveSettings();

  if (zoneStates[i] == STATE_IDLE) {
    verifyTimers[i] = 0;
    zoneStates[i] = STATE_OPENING; 
    zoneTimers[i] = millis();
    lastManualNotify[i] = 0;
    digitalWrite(zones[i].relayPin, LOW);
    Serial.printf("MANUAL: Start Zone %d\n", i+1);
  }
}

void forceZoneStop(int i) {
  if (i < 0 || i >= NUM_ZONES) return;

  if (zoneStates[i] != STATE_IDLE && zoneStates[i] != STATE_CLOSING) {
    zoneStates[i] = STATE_CLOSING; 
    zoneTimers[i] = millis();
    Serial.printf("MANUAL: Stop Zone %d\n", i+1);
  }
}

void stopAllZones() {
  for(int i=0; i<NUM_ZONES; i++) {
    if (zoneStates[i] != STATE_IDLE) forceZoneStop(i);
  }
  irrigationPumpManualState = false;
  digitalWrite(PUMP_PIN, HIGH); 
}

void openValveManual(int i) {
  if (i < 0 || i >= NUM_ZONES) return;
  if (!zones[i].enabled) return;
  if (isZoneCalibrating(i)) return;
  if (!isWaterAvailable()) return;

  currentSystemMode = MODE_MANUAL;
  saveSettings();

  verifyTimers[i] = 0;
  zoneStates[i] = STATE_WATERING;
  zoneTimers[i] = millis();
  lastManualNotify[i] = 0;

  digitalWrite(zones[i].relayPin, LOW);
  irrigationPumpManualState = true;
  digitalWrite(PUMP_PIN, LOW);

  Serial.printf("APP MQTT: Open valve zone %d\n", i + 1);
}

void closeValveManual(int i) {
  if (i < 0 || i >= NUM_ZONES) return;

  digitalWrite(zones[i].relayPin, HIGH);
  zoneStates[i] = STATE_IDLE;
  verifyTimers[i] = 0;

  struct tm timeinfo;
  if(getLocalTime(&timeinfo)) {
    strftime(zones[i].lastWaterTime, 6, "%H:%M", &timeinfo);
  }

  bool anyOpen = false;
  for (int z = 0; z < NUM_ZONES; z++) {
    if (digitalRead(zones[z].relayPin) == LOW) {
      anyOpen = true;
      break;
    }
  }

  if (!anyOpen) {
    irrigationPumpManualState = false;
    digitalWrite(PUMP_PIN, HIGH);
  }

  saveSettings();
  Serial.printf("APP MQTT: Close valve zone %d\n", i + 1);
}

void closeAllValvesManual() {
  for (int i = 0; i < NUM_ZONES; i++) {
    digitalWrite(zones[i].relayPin, HIGH);
    zoneStates[i] = STATE_IDLE;
    verifyTimers[i] = 0;
  }

  irrigationPumpManualState = false;
  digitalWrite(PUMP_PIN, HIGH);
  Serial.println("APP MQTT: Close all valves");
}
