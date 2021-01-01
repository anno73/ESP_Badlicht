#pragma once

namespace ota {

const uint8_t OTA_UPDATE_PASWORD_STR_LEN = 40;  // Must be smaller than IOTWEBCONF_PASSWORD_LEN 
extern char otaUpdatePassword[];

extern void setupArduinoOta();
extern void loopArduinoOta();

}
