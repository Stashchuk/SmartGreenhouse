#include "config.h"
#include "logic.h"
#include "sensors.h"
#include "network.h"

ZoneState zoneStates[NUM_ZONES];       
unsigned long zoneTimers[NUM_ZONES];   
unsigned long verifyTimers[NUM_ZONES]; 
unsigned long nextAllowedActionTime = 0; 

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

void updateWateringLogic() {
  unsigned long currentMillis = millis();
  bool isPumpNeeded = false; 
  
  bool isMechanismBusy = false;
  for (int j = 0; j < NUM_ZONES; j++) {
    if (zoneStates[j] == STATE_OPENING || zoneStates[j] == STATE_CLOSING) {
      isMechanismBusy = true; break; 
    }
  }
  
  bool isQueueLocked = (currentMillis < nextAllowedActionTime);

  for (int i = 0; i < NUM_ZONES; i++) {
    int moisture = getPercent(i);

    switch (zoneStates[i]) {
      case STATE_IDLE:
        if (moisture < zones[i].min) {
          if (verifyTimers[i] == 0) verifyTimers[i] = currentMillis; 
          else if (currentMillis - verifyTimers[i] >= SENSOR_VERIFY_TIME) {
             verifyTimers[i] = 0; 
             zoneStates[i] = STATE_WAITING; 
             sendTelegramMessage(zones[i].topicID, "⚠️ <b>Критично сухо!</b> (" + String(moisture) + "%)\nПочинаю цикл поливу...");
          }
        } else {
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
        if (currentMillis - zoneTimers[i] >= zones[i].wateringTime) {
          zoneStates[i] = STATE_CLOSING; 
          zoneTimers[i] = currentMillis;
        }
        break;

      case STATE_CLOSING:
        if (currentMillis - zoneTimers[i] >= VALVE_CLOSE_DELAY) {
          digitalWrite(zones[i].relayPin, HIGH); 
          zoneStates[i] = STATE_ANALYZING;
          zoneTimers[i] = currentMillis;
          nextAllowedActionTime = currentMillis + QUEUE_DELAY; 
          sendTelegramMessage(zones[i].topicID, "✅ <b>Полив СТОП.</b>\nКлапан закрито. Чекаю вбирання...");
        }
        break;

      case STATE_ANALYZING:
        if (currentMillis - zoneTimers[i] >= zones[i].soakingTime) {
          if (moisture < zones[i].max) {
             zoneStates[i] = STATE_WAITING; 
             String statusText;
             if (moisture < zones[i].min) statusText = "⚠️ Все ще сухо (" + String(moisture) + "%).";
             else statusText = "💧 Вологість " + String(moisture) + "%. Треба до " + String(zones[i].max) + "%.";
             sendTelegramMessage(zones[i].topicID, statusText + "\n🔄 Повторюю цикл поливу!");
          } 
          else {
             zoneStates[i] = STATE_IDLE; 
             sendTelegramMessage(zones[i].topicID, "🆗 <b>Полито повністю!</b> (" + String(moisture) + "%).\nПереходжу в режим сну.");
          }
        }
        break;
    }
  }

  if (isPumpNeeded) digitalWrite(PUMP_PIN, LOW);
  else digitalWrite(PUMP_PIN, HIGH);
}