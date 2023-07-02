/**
  * @file FailSafe.h
  * @version 0.2.3
  * @date 18/12/2020
  * @author German Martin
  * @brief Library to add a simple fail safe mode to any ESP32 or ESP8266 project
  */

#ifndef _BOOTCHECK_h
#define _BOOTCHECK_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "Arduino.h"
#else
	#include "WProgram.h"
#endif

#include "FS.h"

#if !defined FS_USE_FLASH || __DOXYGEN__
#define FS_USE_FLASH                   0  /**< Set to 1 to use File System. Set to 0 to use RTC memory. Only for ESP8266. Does not work with power off.*/
#endif // FS_USE_FLASH

#if FS_USE_FLASH || __DOXYGEN__
#if !defined FS_USE_LITTLEFS || __DOXYGEN__
#define FS_USE_LITTLEFS                  0  /**< Set to 0 to use SPIFFS (Default). Set to 1 to use LittleFS (Recommended). Only for ESP8266, ESP32 always uses SPIFFS*/
#endif // FS_USE_LITTLEFS
#endif // FS_USE_FLASH

    #undef FS_USE_FLASH
    #undef FS_USE_LITTLEFS 
    #define FS_USE_FLASH 1 // Do not modify
    #define FS_USE_LITTLEFS 0 // Do not modify
    #include <SPIFFS.h>
    #include "Update.h"

#ifndef FAIL_SAFE_DEBUG
#define FAIL_SAFE_DEBUG 0 /**< Set to 1 to enable log. If using ESP32 you need to set core log level to WARN at least */
#endif

#if FAIL_SAFE_DEBUG
#ifdef ESP8266
#define DEBUG_INTERFACE Serial
#ifdef DEBUG_INTERFACE
const char* extractFileName (const char* path);
#define DEBUG_LINE_PREFIX() DEBUG_INTERFACE.printf_P (PSTR("[%lu][%s:%d] %s() Heap: %lu | "),millis(),extractFileName(__FILE__),__LINE__,__FUNCTION__,(unsigned long)ESP.getFreeHeap())
#define fsDebug(text,...) DEBUG_LINE_PREFIX();DEBUG_INTERFACE.printf_P(PSTR(text),##__VA_ARGS__);DEBUG_INTERFACE.println()
#endif // DEBUG_INTERFACE
#else  // ESP32
#define DEFAULT_LOG_TAG "FailSafe"
#define fsDebug(format,...) ESP_LOGW (DEFAULT_LOG_TAG,"%d Heap: %6d. " format, millis(), ESP.getFreeHeap(), ##__VA_ARGS__)
#endif // ESP8266
#else
#define fsDebug(text,...)
#endif // FAIL_SAFE_DEBUG


constexpr auto FILENAME = "/failsafe.bin";

#define DEFAULT_BOOT_FLAG_TIMEOUT   10000        /**< Boot flag is reset this time after boot */
#define DEFAULT_OFFSET              0            /**< If RTC is used this is offset. Modify it if your project uses RTC memory too */
#define DEFAULT_MAX_BOOT_CYCLES     3            /**< Number of quick boot loops on which fail safe mode is started */

struct bootFlag_t {
#if !FS_USE_FLASH || __DOXYGEN__
    uint32_t crc;        /**< If RTC is used this field is used to check data integrify */
#endif // FS_USE_FLASH
    int32_t bootCycles; /**< Booth cycle counter */
};

class FailSafeClass
{
 static volatile bool watchdog;
 static hw_timer_t* timer;
 static portMUX_TYPE timerMux;
 static void IRAM_ATTR onTimer();

 protected:
     bootFlag_t bootFlag;               /**< Boot counter */
     uint32_t offset;                   /**< RTC memory offset */

     /**
      * @brief Save boot flag on persistent storage (RTC or SPIFFS)
      * @return Save result, false in case of error
      */
     bool saveFlag ();

     /**
      * @brief Load last boot counter from storage (RTC or SPIFFS)
      * @return Save result, false in case of error
      */
     bool loadFlag ();

     /**
      * @brief Internal loop method in fail safe mode
      */
     void failSafeModeLoop ();

public:

     /**
      * @brief Reset boot counter to 0 and update storage
      */
     void resetFlag ();

     /**
      * @brief Checks if fail safe mode should be activated. This should be called at the beginning of main setup function
      * @param maxBootCycles Number of cycles after device enters in Fail Safe mode
      * @param led Indicates Fail Safe mode status. Normally off (= HIGH)
      * @param memOffset RTC memory offset. This is not used if storage is SPIFFS
      */
     void checkBoot (int maxBootCycles = DEFAULT_MAX_BOOT_CYCLES, int led = -1, uint32_t memOffset = DEFAULT_OFFSET);

     /**
      * @brief Triggers fail safe mode manually from user code. IT can be used if WiFi cannot connect, for instance
      */
     void startFailSafe ();

     /**
      * @brief Internal loop should be called on every main code loop
      * @param timeout Boot flag is reset this time after boot
      */
     void loop (uint32_t timeout = DEFAULT_BOOT_FLAG_TIMEOUT);

     /**
      * @brief Converts current status to a char string
      * @return Textual Fail Safe mode status
      */
     const char* toString ();
};

extern FailSafeClass FailSafe; /**< Static instance */

#endif

