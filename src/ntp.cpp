#include <Arduino.h>
#include <Streaming.h>

// Probably replace with ESPDateTime
// https://blog.mcxiaoke.com/ESPDateTime/
// https://github.com/mcxiaoke/ESPDateTime

#include "ntp.h"

char ntpServer[NTP_SERVER_STR_LEN] = "192.168.2.7";

char ntpTzOffset[NTP_TZ_OFFSET_STR_LEN] = "2";
int ntpTzOffsetInt;

unsigned int ntpLocalPort = 2390;
const int ntpPacketSize = 48;
unsigned long ntpUpdateInterval = 60000;

byte packetBuffer[ntpPacketSize];

WiFiUDP ntpUdp;
NTPClient * ntpClient = NULL;

// wifiConnected callback indicates that now we can/should issue a NTP update
bool ntpNeedUpdate = false;
// Set by mqtt - do not know how to set via NTPClient...
bool timeValid = false;


void setupNtp(void);



bool updateNtp() {
  bool rc = false;

  if (ntpClient == NULL)
    setupNtp();

  if (ntpClient == NULL) {
    return false;
  }
  
  rc = ntpClient->update();

  return rc;
}

//
// Called by main setup
//
void setupNtp () {
  Serial << F("Setup NTP\n");

  ntpClient = new NTPClient(ntpUdp, ntpServer);
  
  ntpClient->begin();
  ntpClient->setTimeOffset(ntpTzOffsetInt * 3600);
  ntpClient->setUpdateInterval(ntpUpdateInterval);
} // setupNtp

//
// Called by main loop
//
void loopNtp () {

//  static unsigned int oldSec = 0;
  static unsigned int oldMin = 0;
//  unsigned int nowSec, nowMin, nowHour;
  unsigned int nowSec, nowMin;
  
  if ( ntpClient == NULL ) {
    return;
  }

  bool rc = false;

  // Regular loop or forced update required (e.g. WiFi reconnect, ...)
  if (ntpNeedUpdate) {
    rc = ntpClient->forceUpdate();
    if (rc) {
      ntpNeedUpdate = false;
    }
  } else {  
    rc = ntpClient->update();
  }
  

  nowSec = ntpClient->getSeconds();
  nowMin = ntpClient->getMinutes();
//  nowHour = ntpClient->getHours();

  // Print current time once in a while 
  if ((nowSec == 0) && (oldMin != nowMin)) {
    oldMin = nowMin;
    
    Serial << F("NTP time is ") << ntpClient->getFormattedTime() << endl;
  }

} // loopNtp
