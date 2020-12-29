#include <Arduino.h>

//#include <FS.h>

//#include <TimeLib.h>
#include <Streaming.h>

// https://arduinojson.org/v6/doc/
//#define ARDUINOJSON_USE_LONG_LONG 1
//#include <ArduinoJson.h>

#include "global.h"
#include "mqtt.h"
#include "iotWebConf_.h"
#include "ota.h"
#include "ntp.h"
#include "rgb_pwm.h"

//----------------------------------------------------------------------
// DoubleResetDetector configuration
// Number of seconds after reset during which a
// subseqent reset will be considered a double reset.
#define DRD_TIMEOUT 2

// RTC Memory Address for the DoubleResetDetector to use
#define DRD_ADDRESS 0

#define DRD_DEBUG Serial

#ifdef ESP8266
  #define ESP8266_DRD_USE_RTC   true
#else
  #error this is for esp8266 only
#endif

#define ESP_DRD_USE_LITTLEFS    false
#define ESP_DRD_USE_SPIFFS      false
#define ESP_DRD_USE_EEPROM      false

#include <ESP_DoubleResetDetector.h>
//----------------------------------------------------------------------

//#define BTN_FLASH 0
//#define BNT_USER 16


extern "C" {
#include "user_interface.h"
}


bool needReset = false;


DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);



/* unsigned long lastMqttConnectionAttempt = 0;
int needAction = NO_ACTION;
int state = LOW;
unsigned long lastAction = 0;
char mqttActionTopic[STRING_LEN];
char mqttStatusTopic[STRING_LEN];
 */
//
// Arduino Setup
//
void setup() 
{
  bool doubleReset = false;

  Serial.begin(2000000);
  Serial << endl << appName << F("starting up...\n");

  if (drd.detectDoubleReset()) {
    Serial << F("Double reset detected\n");
    doubleReset = true;
  } else {
    Serial << F("No double reset detected. Continue normally\n");
    doubleReset = false;
  }

  if (doubleReset) {

  }

  setupIotWebConf();
  
  setupMqttClient();

  setupNtp();

  ota::setupArduinoOta();

  /* 
  Serial << F("Initialize SPIFFS...\n");
  SPIFFS.begin();
  {
    FSInfo fs_info;
    SPIFFS.info(fs_info);
    Serial << F("totalBytes:    ") << fs_info.totalBytes << endl;
    Serial << F("usedBytes:     ") << fs_info.usedBytes << endl;
    Serial << F("blockSize:     ") << fs_info.blockSize << endl;
    Serial << F("pageSize:      ") << fs_info.pageSize << endl;
    Serial << F("maxOpenFiles:  ") << fs_info.maxOpenFiles << endl;
    Serial << F("maxPathLength: ") << fs_info.maxPathLength << endl;
  }
  Serial << F("SPIFFS done.") << endl;
*/
  Serial << F("Heap: ") << system_get_free_heap_size() << endl;

  Serial << F("Setup RGB ...") << endl;
  setupRgb();
  Serial << F("RGB done.") << endl;
}

void loop() 
{

  loopIotWebConf();
  ota::loopArduinoOta();

  drd.loop();

  loopMqtt();

  if (needReset) {
    Serial << F("Reboot requested\n");
//    iotWebConf.delay(1000);
    delay(1000);
    ESP.restart();
  }

  loopNtp();
 
  loopRgb();
}


