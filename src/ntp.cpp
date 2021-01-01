#include <Arduino.h>
#include <Streaming.h>

/*
    Meaningful native example found here: https://www.esp8266.com/viewtopic.php?p=89714#p89714

    Probably extend with NTP server name provided by DHCP.

    .platformio/packages/framework-arduinoespressif8266/tools/sdk/lwip/include/lwip/sntp.h:void sntp_stop(void);
    .platformio/packages/framework-arduinoespressif8266/tools/sdk/lwip/src/core/sntp.c:sntp_stop(void)


    https://github.com/esp8266/Arduino/blob/master/libraries/esp8266/examples/NTP-TZ-DST/NTP-TZ-DST.ino

*/

#include <TZ.h>
#include <coredecls.h> // settimeofday_cb()

#include "ntp.h"

char ntpServer[NTP_SERVER_STR_LEN] = "192.168.2.7";

char ntpTzOffset[NTP_TZ_OFFSET_STR_LEN] = "2";
int ntpTzOffsetInt;

// Global variables for Time
tm *NOW_TM; // pointer to a tm struct;
time_t NOW; // global holding current datetime as Epoch

// wifiConnected callback indicates that now we can/should issue a NTP update
bool ntpNeedUpdate = false;

bool timeValid = false;

// Forward declatations
String dateTimeStr(const char *pattern = (char *)"%Y-%m-%d %H:%M:%S");
String dateTimeStr(time_t epochtime = time(nullptr), const char *pattern = (char *)"%Y-%m-%d %H:%M:%S");

//void ntp_time_set_cb(bool bySntp)
void ntp_time_set_cb()
{
//  Serial << F("NTP Callback called. Auto trigger: ") << bySntp << endl;
  Serial << F("NTP Callback called at ") << dateTimeStr("%Y-%m-%d %H:%M:%S") << endl;

  timeValid = true;
}

//
// Called by main setup
//
void setupNtp()
{
  Serial << F("Setup NTP") << endl;

  settimeofday_cb(ntp_time_set_cb);

  // void configTime(int timezone, int daylightOffset_sec, const char* server1, const char* server2, const char* server3)
  configTime(MYTZ, ntpServer);

#if 1
  // Trigger async time update. timeValid will be set in ntp_time_set_cb when done.
  NOW = time(nullptr);
#else
  unsigned long t0 = millis();

  Serial << F("NTP: Synching Time over NTP ");
  // time() always returns some value.
  // If uninitialized returns seconds since startup.
  // So check for a meaningfull offset
  while ((NOW = time(nullptr)) < NTP_MIN_TIME_EPOCH)
  {
    // warning : no time out. May loop here forever
    delay(20);
    Serial << (".");
  }
  Serial << endl;

  unsigned long t1 = millis() - t0;
  Serial << F("NTP: time first synch took ") << t1 << "ms" << endl;
  //  Serial << F("Current date: ") << dateTimeStr(time(nullptr), "%d-%m-%Y %H:%M:%S") << endl;
  Serial << F("NTP: Current date: ") << dateTimeStr(time(nullptr)) << endl;

  timeValid = true;
#endif

} // setupNtp

//
// Called by main loop
//
void loopNtp()
{
  return;
} // loopNtp

// returns String with pattern from time_t time
// formats : https://www.cplusplus.com/reference/ctime/strftime/
// default pattern is "%d-%m-%Y %H:%M:%S"
String dateTimeStr(time_t epochtime, const char *pattern)
{
  tm *lt;
  lt = localtime(&epochtime);
  char buff[30];
  strftime(buff, 30, pattern, lt);
  return buff;
}

String dateTimeStr(const char *pattern)
{
  return dateTimeStr(time(nullptr), pattern);
}
