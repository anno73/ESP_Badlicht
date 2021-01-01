#pragma once

namespace ota {

#define OTA_UPDATE_PASWORD_STR_LEN 40
extern char otaUpdatePassword[];

extern void setupArduinoOta();
extern void loopArduinoOta();

}
