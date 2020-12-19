#include <ArduinoOTA.h>
#include <Streaming.h>

#include "ota.h"
#include "global.h"
#include "iotWebConf_.h"

char otaUpdatePassword[OTA_UPDATE_PASWORD_STR_LEN] = "";

//
// OTA
//
void setupArduinoOta() {
  Serial << F("OTA setup\n");

  ArduinoOTA.setHostname(appName);

  if (otaUpdatePassword[0] != '\0') {
    ArduinoOTA.setPassword(otaUpdatePassword);
  }

  ArduinoOTA.onStart([]() {
    String type;

    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else
      type = "filesystem";
    Serial << F("OTA update start updating ") << type << endl;
  }); // .onStart

  ArduinoOTA.onEnd([]() {
    Serial << F("\nOTA update end\n");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial << F("Auth failed");
    else if (error == OTA_BEGIN_ERROR) Serial << F("Begin failed");
    else if (error == OTA_CONNECT_ERROR) Serial << F("Connect failed");
    else if (error == OTA_RECEIVE_ERROR) Serial << F("Receive failed");
    else if (error == OTA_END_ERROR) Serial << F("End failed");
    else Serial << F("Unknown failure");
    Serial << endl;
  });

  ArduinoOTA.begin();
} // setupArduinoOTA

void loopArduinoOta()
{
  ArduinoOTA.handle();
}