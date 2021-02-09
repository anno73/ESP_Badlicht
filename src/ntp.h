#pragma once

#define MYTZ TZ_Europe_Paris    // As found in %USERPROFILE%\.platformio\packages\framework-arduinoespressif8266\cores\esp8266\TZ.h

const int32_t NTP_MIN_TIME_EPOCH = 1577836800; // 2020-01-01-00:00:00

const uint8_t NTP_SERVER_STR_LEN = 64;
extern char ntpServer[];

const uint8_t NTP_TZ_OFFSET_STR_LEN = 4;
extern char ntpTzOffset[];
extern int ntpTzOffsetInt;

// wifiConnected callback indicates that now we can/should issue a NTP update
extern bool ntpNeedUpdate;

// Global variables for Time
extern tm *NOW_TM; // pointer to a tm struct;
extern time_t NOW; // global holding current datetime as Epoch

extern void setupNtp();
extern void loopNtp();
//extern bool timeValid;

extern String dateTimeStr(const char *);
extern String dateTimeStr(time_t, const char *);

