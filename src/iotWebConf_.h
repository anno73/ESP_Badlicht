#pragma once

// https://github.com/prampec/IotWebConf
#define IOTWEBCONF_DEBUG_DISABLED
#include <IotWebConf.h>
extern IotWebConf iotWebConf;

extern ESP8266WebServer webServer;
extern DNSServer dnsServer;
extern WiFiClient wifiClient;
extern const char appName[];

extern void setupIotWebConf();
extern void loopIotWebConf();
extern void handleRoot();
