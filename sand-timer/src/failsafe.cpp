/**
 * @file FailSafe.cpp
 * @version 0.2.3
 * @date 18/12/2020
 * @author German Martin
 * @brief Library to add a simple fail safe mode to any ESP32 or ESP8266 project
 */

#include "failsafe.h"

#include <SPIFFS.h>

#include "FS.h"
#include "esp_ota_ops.h"
#define FAILSAFE_FS SPIFFS

#include <ArduinoOTA.h>

#if FAIL_SAFE_DEBUG
const char* IRAM_ATTR extractFileName(const char* path) {
  size_t i = 0;
  size_t pos = 0;
  char* p = (char*)path;
  while (*p) {
    i++;
    if (*p == '/' || *p == '\\') {
      pos = i;
    }
    p++;
  }
  return path + pos;
}
#endif  // FAIL_SAFE_DEBUG

void FailSafeClass::resetFlag() {
  fsDebug("Reset flag");
  bootFlag.bootCycles = 0;
  saveFlag();
}
bool FailSafeClass::loadFlag() {
  if (!FAILSAFE_FS.begin()) {
    FAILSAFE_FS.format();
    resetFlag();
    return false;
  }

  fsDebug("Started Filesystem for loading");

  if (!FAILSAFE_FS.exists(FILENAME)) {
    fsDebug("File %s not found", FILENAME);
    return true;
  }

  fsDebug("Opening %s file", FILENAME);
  File configFile = FAILSAFE_FS.open(FILENAME, "r");
  if (!configFile) {
    fsDebug("Error opening %s", FILENAME);
    resetFlag();
    return false;
  }

  fsDebug("%s opened", FILENAME);
  size_t size = configFile.size();

  if (size < sizeof(bootFlag_t)) {
    fsDebug("File size error");
    return false;
  }

  configFile.read((uint8_t*)&bootFlag, sizeof(bootFlag));
  configFile.close();

  fsDebug("Read RTCData: %08X", bootFlag.bootCycles);

  return true;
}

bool FailSafeClass::saveFlag() {
  // if (!FAILSAFE_FS.begin ()) {
  //     fsDebug ("Error opening FS for saving");
  //     return false;
  // }

  fsDebug("Started Filesystem for saving");

  File configFile = FAILSAFE_FS.open(FILENAME, "w");
  if (!configFile) {
    fsDebug("failed to open config file %s for writing", FILENAME);
    return false;
  }

  configFile.write((uint8_t*)&bootFlag, sizeof(bootFlag));
  configFile.flush();
  configFile.close();

  fsDebug("Write FSData: %d", bootFlag.bootCycles);

  return true;
}

void IRAM_ATTR FailSafeClass::onTimer() {
  fsDebug("Failsafe timer triggered");
  portENTER_CRITICAL_ISR(&FailSafeClass::timerMux);
  bool trigger = FailSafeClass::watchdog;
  FailSafeClass::watchdog = true;
  portEXIT_CRITICAL_ISR(&FailSafeClass::timerMux);
  if (trigger) {
    FailSafe.startFailSafe();
  }
}

void FailSafeClass::checkBoot(int maxBootCycles, int led, uint32_t memOffset) {
  offset = memOffset;
  if (loadFlag()) {
    if (bootFlag.bootCycles >= maxBootCycles) {
      FailSafe.startFailSafe();
    } else {
      bootFlag.bootCycles++;
      saveFlag();
    }
  }
  fsDebug("Fail boot is %s", toString());

  FailSafeClass::watchdog = true;
  FailSafeClass::timerMux = portMUX_INITIALIZER_UNLOCKED;
  FailSafeClass::timer =
      timerBegin(0, 80, true);  // timer 0, prescalar: 80, UP counting
  timerAttachInterrupt(FailSafeClass::timer, &FailSafeClass::onTimer,
                       true);  // Attach interrupt
  timerAlarmWrite(FailSafeClass::timer, 1000000 * 60, true);
  timerAlarmEnable(FailSafeClass::timer);
}

void FailSafeClass::startFailSafe() {
  if (esp_ota_mark_app_invalid_rollback_and_reboot() ==
      ESP_ERR_OTA_ROLLBACK_FAILED) {
    resetFlag();
  }
}

void FailSafeClass::loop(uint32_t timeout) {
  if (bootFlag.bootCycles > 0) {
    if (millis() > timeout) {
      fsDebug("Flag timeout %d", millis());
      resetFlag();
    }
  }

  portENTER_CRITICAL_ISR(&FailSafeClass::timerMux);
  FailSafeClass::watchdog = false;
  portEXIT_CRITICAL_ISR(&FailSafeClass::timerMux);
}

volatile bool FailSafeClass::watchdog = true;
portMUX_TYPE FailSafeClass::timerMux = portMUX_INITIALIZER_UNLOCKED;
hw_timer_t* FailSafeClass::timer;
FailSafeClass FailSafe;
