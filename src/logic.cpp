#include "config.h"
#include "logic.h"
#include "sensors.h"
#include "network.h"

ZoneState zoneStates[NUM_ZONES];       
unsigned long zoneTimers[NUM_ZONES];   
unsigned long verifyTimers[NUM_ZONES]; 
unsigned long nextAllowedActionTime = 0; 

// Змінна для відстеження часу останнього нагадування
unsigned long lastManualNotify[NUM_ZONES] = {0, 0, 0, 0};

void setupLogic() {
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, HIGH); 

  for(int i=0; i<NUM_ZONES; i++) {
    pinMode(zones[i].relayPin, OUTPUT);
    digitalWrite(zones[i].relayPin, HIGH); 
    zoneStates[i] = STATE_IDLE;
    zoneTimers[i] = 0;
    verifyTimers[i] = 0;
  }
}

ZoneState getZoneState(int i) { return zoneStates[i]; }
unsigned long getZoneTimer(int i) { return zoneTimers[i]; }
unsigned long getVerifyTimer(int i) { return verifyTimers[i]; }

String getZoneStatusText(int i, int currentMoisture) {
    if (zoneStates[i] == STATE_WATERING) return "💦 (Полив)";
    if (zoneStates[i] == STATE_ANALYZING) return "⏳ (Вбирання)";
    if (zoneStates[i] == STATE_WAITING) return "🕒 (В черзі)";
    if (zoneStates[i] == STATE_OPENING) return "⚙️ (Відкриття)";
    if (zoneStates[i] == STATE_IDLE && verifyTimers[i] > 0) return "🧐 (Перевірка...)";
    
    if (currentMoisture < zones[i].min) return "💧 (Сухо)";
    if (currentMoisture > zones[i].max) return "🌊 (Мокро)";
    return "✅ (Норма)";
}

// Функція перевірки довгих поливів у ручному режимі
void checkManualAlerts() {
  if (currentSystemMode == MODE_MANUAL) {
    unsigned long currentMillis = millis();
    for (int i = 0; i < NUM_ZONES; i++) {
      if (zoneStates[i] == STATE_WATERING) {
        unsigned long activeTimeMin = (currentMillis - zoneTimers[i]) / 60000;
        // Якщо працює більше 30 хв і ми ще не нагадували про цей 30-хвилинний відрізок
        if (activeTimeMin >= 30 && (activeTimeMin / 30) > (lastManualNotify[i] / 30)) {
           lastManualNotify[i] = activeTimeMin;
           sendTelegramMessage(TOPIC_MAIN, "⚠️ <b>УВАГА!</b>\nЗона <b>" + String(zones[i].name) + "</b> поливає вже " + String(activeTimeMin) + " хв!\nНе забудьте вимкнути.");
        }
      } else {
        lastManualNotify[i] = 0; // Скидаємо, якщо не поливає
      }
    }
  }
}

void updateWateringLogic() {
  unsigned long currentMillis = millis();
  bool isPumpNeeded = false; 
  
  // Викликаємо нагадування
  checkManualAlerts();

  bool isMechanismBusy = false;
  for (int j = 0; j < NUM_ZONES; j++) {
    if (zoneStates[j] == STATE_OPENING || zoneStates[j] == STATE_CLOSING) {
      isMechanismBusy = true; 
      break; 
    }
  }
  
  bool isQueueLocked = (currentMillis < nextAllowedActionTime);

  for (int i = 0; i < NUM_ZONES; i++) {
    int moisture = getPercent(i);

    switch (zoneStates[i]) {
      case STATE_IDLE:
        // 👇 ДОДАНО ЗАХИСТ: Автоматика працює тільки в MODE_AUTO
        if (currentSystemMode == MODE_AUTO && moisture < zones[i].min) {
          if (verifyTimers[i] == 0) {
            verifyTimers[i] = currentMillis; 
            Serial.printf("Z%d: Start verification. Moisture: %d%%\n", i+1, moisture);
          } 
          else if (currentMillis - verifyTimers[i] >= SENSOR_VERIFY_TIME) {
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
        if (currentMillis - zoneTimers[i] >= VALVE_OPEN_DELAY) {
          zoneStates[i] = STATE_WATERING; 
          zoneTimers[i] = currentMillis;
          nextAllowedActionTime = currentMillis + QUEUE_DELAY; 
          sendTelegramMessage(zones[i].topicID, "💦 <b>Полив СТАРТ!</b>\nТривалість: " + String(zones[i].wateringTime / 60000) + " хв");
        }
        break;

      case STATE_WATERING:
        isPumpNeeded = true; 
        // В автоматичному режимі зупиняємо по таймеру. В ручному - поливаємо доки не скасують.
        if (currentSystemMode == MODE_AUTO && (currentMillis - zoneTimers[i] >= zones[i].wateringTime)) {
          zoneStates[i] = STATE_CLOSING; 
          zoneTimers[i] = currentMillis;
        }
        break;

      case STATE_CLOSING:
        if (currentMillis - zoneTimers[i] >= VALVE_CLOSE_DELAY) {
          digitalWrite(zones[i].relayPin, HIGH); 
          zoneStates[i] = (currentSystemMode == MODE_AUTO) ? STATE_ANALYZING : STATE_IDLE;
          zoneTimers[i] = currentMillis;
          nextAllowedActionTime = currentMillis + QUEUE_DELAY; 
          if (currentSystemMode == MODE_AUTO)
            sendTelegramMessage(zones[i].topicID, "✅ <b>Полив СТОП.</b>\nКлапан закрито. Чекаю вбирання...");
          else
            sendTelegramMessage(zones[i].topicID, "⏹ <b>Ручний полив завершено.</b>\nКлапан закрито.");
        }
        break;

      case STATE_ANALYZING:
        if (currentSystemMode == MODE_MANUAL) { zoneStates[i] = STATE_IDLE; break; } // Якщо перемкнули в ручний - виходимо зі стану
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

  if (isPumpNeeded) digitalWrite(PUMP_PIN, LOW);
  else digitalWrite(PUMP_PIN, HIGH);
}

// ==========================================
// 🎮 РУЧНЕ КЕРУВАННЯ
// ==========================================

bool isZoneActive(int i) {
  return (zoneStates[i] != STATE_IDLE && zoneStates[i] != STATE_ANALYZING);
}

void forceZoneStart(int i) {
  if (i < 0 || i >= NUM_ZONES) return;
  // Дозволяємо ручний пуск ТІЛЬКИ в ручному режимі (як ми домовлялися)
  if (currentSystemMode == MODE_MANUAL && zoneStates[i] == STATE_IDLE) {
     verifyTimers[i] = 0;
     zoneStates[i] = STATE_OPENING; 
     zoneTimers[i] = millis();
     lastManualNotify[i] = 0; // Скидаємо лічильник нагадувань
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
  digitalWrite(PUMP_PIN, HIGH); 
}