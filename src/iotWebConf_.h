#ifndef MY_IOTWEBCONF_H
#define MY_IOTWEBCONF_H

// https://github.com/prampec/IotWebConf
//#define IOTWEBCONF_DEBUG_DISABLED
#include <IotWebConf.h>
extern IotWebConf iotWebConf;

#include <ESP8266WebServer.h>
extern ESP8266WebServer webServer;

#include <DNSServer.h>
extern DNSServer dnsServer;

extern WiFiClient wifiClient;
extern const char appName[];


extern void setupIotWebConf();
extern void loopIotWebConf();
extern void handleRoot();

#endif