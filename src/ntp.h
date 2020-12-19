#ifndef MY_NTP_H
#define MY_NTP_H

#include <WiFiUdp.h>
// https://github.com/arduino-libraries/NTPClient
// BEWARE: there are some forks and libs with similar name!
#include <NTPClient.h>

#define NTP_SERVER_STR_LEN 64
extern char ntpServer[];

#define NTP_TZ_OFFSET_STR_LEN 4
extern char ntpTzOffset[];
extern int ntpTzOffsetInt;

//unsigned int ntpLocalPort = 2390;
//const int ntpPacketSize = 48;
//unsigned long ntpUpdateInterval = 60000;

//byte packetBuffer[ntpPacketSize];

extern WiFiUDP ntpUdp;
extern NTPClient * ntpClient;

// wifiConnected callback indicates that now we can/should issue a NTP update
extern bool ntpNeedUpdate;


extern void setupNtp();
extern void loopNtp();
extern bool timeValid;

#endif