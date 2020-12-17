#ifndef MY_NTP_H
#define MY_NTP_H

// Probably replace with ESPDateTime
// https://blog.mcxiaoke.com/ESPDateTime/
// https://github.com/mcxiaoke/ESPDateTime


#include <WiFiUdp.h>
// https://github.com/arduino-libraries/NTPClient
// BEWARE: there are some forks and libs with similar name!
#include <NTPClient.h>


char ntpServer[64] = "192.168.2.7";
char ntpTzOffset[4] = "2";
int ntpTzOffsetInt;
unsigned int ntpLocalPort = 2390;
const int ntpPacketSize = 48;
unsigned long ntpUpdateInterval = 60000;

byte packetBuffer[ntpPacketSize];

WiFiUDP ntpUdp;
NTPClient * ntpClient = NULL;

// wifiConnected callback indicates that now we can/should issue a NTP update
boolean ntpNeedUpdate = false;
// Set by mqtt - do not know how to set via NTPClient...
boolean timeValid = false;

#endif