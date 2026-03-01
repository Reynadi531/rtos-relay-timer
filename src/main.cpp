#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <semphr.h>
#include <RTClib.h>
#include <EEPROM.h>

RTC_DS3231 rtc;
SemaphoreHandle_t configMutex;

#define EEPROM_MAGIC_NUMBER 0x4C 
#define EEPROM_START_ADDR 0

enum SchedMode { MODE_NONE, MODE_FIXED, MODE_INTERVAL };

struct RelayConfig {
  uint8_t pin;
  SchedMode mode;
  uint8_t onHour;        
  uint8_t onMinute;      
  uint32_t intervalSecs; 
  uint32_t durationSecs; 
  
  bool isON;
  uint32_t lastOnEpoch;
  uint32_t nextIntervalEpoch;
  bool isManualOverride;
};

const int RELAY_COUNT = 4;
RelayConfig relays[RELAY_COUNT] = {
  {12, MODE_FIXED, 9, 0, 0, 120, false, 0, 0, false},
  {11, MODE_FIXED, 10, 30, 0, 120, false, 0, 0, false},
  {10, MODE_FIXED, 14, 15, 0, 120, false, 0, 0, false},
  {9,  MODE_FIXED, 17, 0, 0, 120, false, 0, 0, false}
};

void TaskSerialMonitor(void *pvParameters);
void TaskRelayControl(void *pvParameters);
void saveConfigToEEPROM();
void loadConfigFromEEPROM();

void setup() {
  Serial.begin(9600);
  while (!Serial) {}

  if (!rtc.begin()) {
    Serial.println(F("Couldn't find RTC"));
    while (1); 
  }
  
  if (rtc.lostPower()) {
    Serial.println(F("RTC lost power, setting to compile time."));
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  DateTime compiledTime = DateTime(F(__DATE__), F(__TIME__));
  if (rtc.now() < compiledTime) {
    Serial.println(F("New firmware detected. Syncing RTC to compile time..."));
    rtc.adjust(compiledTime);
  } else {
    Serial.println(F("RTC time is current."));
  }

  loadConfigFromEEPROM();

  for (int i = 0; i < RELAY_COUNT; i++) {
    pinMode(relays[i].pin, OUTPUT);
    digitalWrite(relays[i].pin, LOW);
  }

  configMutex = xSemaphoreCreateMutex();

  if (configMutex != NULL) {
    xTaskCreate(TaskSerialMonitor, "Serial", 160, NULL, 1, NULL);
    xTaskCreate(TaskRelayControl, "Control", 192, NULL, 2, NULL);
  }

  Serial.println(F("System Ready. FreeRTOS Scheduler Starting."));
}

void loop() {
}

void saveConfigToEEPROM() {
  EEPROM.update(EEPROM_START_ADDR, EEPROM_MAGIC_NUMBER);
  EEPROM.put(EEPROM_START_ADDR + 1, relays);
  Serial.println(F("Configuration saved to EEPROM."));
}

void loadConfigFromEEPROM() {
  uint8_t magic = EEPROM.read(EEPROM_START_ADDR);
  
  if (magic == EEPROM_MAGIC_NUMBER) {
    EEPROM.get(EEPROM_START_ADDR + 1, relays);
    for (int i = 0; i < RELAY_COUNT; i++) {
      relays[i].isON = false;
      relays[i].lastOnEpoch = 0;
      relays[i].nextIntervalEpoch = 0;
      relays[i].isManualOverride = false; 
    }
    Serial.println(F("Loaded schedules from EEPROM."));
  } else {
    Serial.println(F("No saved data found. Saving defaults to EEPROM."));
    saveConfigToEEPROM();
  }
}

void TaskRelayControl(void *pvParameters) {
  (void) pvParameters;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = 1000 / portTICK_PERIOD_MS; 

  for (;;) {
    DateTime now = rtc.now();
    uint32_t currentEpoch = now.unixtime();

    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
      for (int i = 0; i < RELAY_COUNT; i++) {
        
        if (relays[i].isON) {
          if (!relays[i].isManualOverride && (currentEpoch - relays[i].lastOnEpoch >= relays[i].durationSecs)) {
            relays[i].isON = false;
            digitalWrite(relays[i].pin, LOW);
            Serial.print(F("Turning OFF Relay ")); Serial.println(i + 1);
          }
        } 
        else {
          bool trigger = false;

          if (relays[i].mode == MODE_FIXED) {
            if (now.hour() == relays[i].onHour && now.minute() == relays[i].onMinute) {
               if (currentEpoch - relays[i].lastOnEpoch > 60) {
                 trigger = true;
               }
            }
          } 
          else if (relays[i].mode == MODE_INTERVAL) {
            if (relays[i].nextIntervalEpoch == 0) {
              relays[i].nextIntervalEpoch = currentEpoch + relays[i].intervalSecs;
            } else if (currentEpoch >= relays[i].nextIntervalEpoch) {
              trigger = true;
              relays[i].nextIntervalEpoch = currentEpoch + relays[i].intervalSecs;
            }
          }

          if (trigger) {
            relays[i].isON = true;
            relays[i].lastOnEpoch = currentEpoch;
            digitalWrite(relays[i].pin, HIGH);
            Serial.print(F("Turning ON Relay ")); Serial.println(i + 1);
          }
        }
      }
      xSemaphoreGive(configMutex);
    }
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

void TaskSerialMonitor(void *pvParameters) {
  (void) pvParameters;
  char buffer[32];
  uint8_t idx = 0;

  for (;;) {
    while (Serial.available() > 0) {
      char c = Serial.read();
      
      if (c == '\n' || c == '\r') {
        buffer[idx] = '\0';
        if (idx > 0) {
          char cmd = buffer[0];
          
          if (cmd == 'S') { 
            if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
              Serial.println(F("["));
              for (int j = 0; j < RELAY_COUNT; j++) {
                Serial.print(F("  {"));
                Serial.print(F("\"id\":")); Serial.print(j + 1); Serial.print(F(","));
                Serial.print(F("\"pin\":")); Serial.print(relays[j].pin); Serial.print(F(","));
                Serial.print(F("\"mode\":")); Serial.print(relays[j].mode); Serial.print(F(","));
                Serial.print(F("\"onHour\":")); Serial.print(relays[j].onHour); Serial.print(F(","));
                Serial.print(F("\"onMinute\":")); Serial.print(relays[j].onMinute); Serial.print(F(","));
                Serial.print(F("\"intervalSecs\":")); Serial.print(relays[j].intervalSecs); Serial.print(F(","));
                Serial.print(F("\"durationSecs\":")); Serial.print(relays[j].durationSecs); Serial.print(F(","));
                Serial.print(F("\"isON\":")); Serial.print(relays[j].isON ? F("true") : F("false")); Serial.print(F(","));
                Serial.print(F("\"isManualOverride\":")); Serial.print(relays[j].isManualOverride ? F("true") : F("false"));
                Serial.print(F("}"));
                if (j < RELAY_COUNT - 1) Serial.println(F(","));
                else Serial.println();
              }
              Serial.println(F("]"));
              xSemaphoreGive(configMutex);
            }
          } 
          else if (cmd == 'C') {
            if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
              for (int j = 0; j < RELAY_COUNT; j++) {
                relays[j].mode = MODE_NONE;
                relays[j].onHour = 0;
                relays[j].onMinute = 0;
                relays[j].intervalSecs = 0;
                relays[j].durationSecs = 0;
                
                relays[j].isON = false;
                relays[j].lastOnEpoch = 0;
                relays[j].nextIntervalEpoch = 0;
                relays[j].isManualOverride = false;
                digitalWrite(relays[j].pin, LOW);
              }
              saveConfigToEEPROM(); 
              Serial.println(F("System Configuration Cleared & Reset."));
              xSemaphoreGive(configMutex);
            }
          }
          else {
            int id = buffer[2] - '1'; 
            
            if (id >= 0 && id < RELAY_COUNT) {
              if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
                if (cmd == 'F') { 
                  int h, m, d;
                  sscanf(buffer, "F %*d %d %d %d", &h, &m, &d);
                  relays[id].mode = MODE_FIXED;
                  relays[id].onHour = h;
                  relays[id].onMinute = m;
                  relays[id].durationSecs = d * 60UL; 
                  Serial.print(F("Relay ")); Serial.print(id+1); Serial.println(F(" set to FIXED."));
                  saveConfigToEEPROM(); 
                } 
                else if (cmd == 'I') { 
                  long inv, d; 
                  sscanf(buffer, "I %*d %ld %ld", &inv, &d);
                  relays[id].mode = MODE_INTERVAL;
                  relays[id].intervalSecs = inv;  
                  relays[id].durationSecs = d;    
                  relays[id].nextIntervalEpoch = rtc.now().unixtime() + relays[id].intervalSecs;
                  Serial.print(F("Relay ")); Serial.print(id+1); Serial.println(F(" set to INTERVAL."));
                  saveConfigToEEPROM(); 
                } 
                else if (cmd == 'M') { 
                  relays[id].isON = !relays[id].isON;
                  relays[id].isManualOverride = relays[id].isON;
                  relays[id].lastOnEpoch = rtc.now().unixtime();
                  digitalWrite(relays[id].pin, relays[id].isON ? HIGH : LOW);
                  Serial.print(F("Relay "));
                  Serial.print(id + 1);
                  Serial.println(relays[id].isON ? F(" turned ON (MANUAL LATCH).") : F(" turned OFF (MANUAL)."));
                }
                
                xSemaphoreGive(configMutex);
              }
            }
          }
          idx = 0; 
        }
      } else {
        if (idx < 31) buffer[idx++] = c;
      }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}