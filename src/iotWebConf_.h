#pragma once

// #define IOTWEBCONF_DEBUG_DISABLED <- Do not do this. Define it in platformio.ini as described in
// https://github.com/prampec/IotWebConf/blob/master/doc/HackingGuide.md#compile-time-configuration
#include <IotWebConf.h>
extern IotWebConf iotWebConf;

extern ESP8266WebServer webServer;
extern DNSServer dnsServer;
extern WiFiClient wifiClient;
extern const char appName[];

extern void setupIotWebConf();
extern void loopIotWebConf();
//extern void handleRoot();
