#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <DNSServer.h>
//#include <MQTT.h>
//#include <IotWebConf.h>
#include <FS.h>
#include <Streaming.h>
#include "mqtt.h"
#include "iotWebConf.h"
#include "ota.h"
#include "ntp.h"

//----------------------------------------------------------------------
// DoubleResetDetector configuration
// Number of seconds after reset during which a
// subseqent reset will be considered a double reset.
#define DRD_TIMEOUT 2

// RTC Memory Address for the DoubleResetDetector to use
#define DRD_ADDRESS 0

#define DRD_DEBUG Serial

#ifdef ESP8266
  #define ESP8266_DRD_USE_RTC   true
#endif

#define ESP_DRD_USE_LITTLEFS    false
#define ESP_DRD_USE_SPIFFS      false
#define ESP_DRD_USE_EEPROM      false

#include <ESP_DoubleResetDetector.h>
//----------------------------------------------------------------------

#define BTN_FLASH 0
#define BNT_USER 16


extern "C" {
#include "user_interface.h"
}

// -- Initial name of the Thing. Used e.g. as SSID of the own Access Point.
const char appName[] = "BAD_RGB";

// -- Initial password to connect to the Thing, when it creates an own Access Point.
const char wifiInitialApPassword[] = "12345678";

#define STRING_LEN 128

// -- Configuration specific key. The value should be modified if config structure was changed.
#define IOTWC_CONFIG_VERSION "BADRGB_001"

// -- When BUTTON_PIN is pulled to ground on startup, the Thing will use the initial
//      password to build an AP. (E.g. in case of lost password)
#define IOTWC_BUTTON_PIN 2

// -- Status indicator pin.
//      First it will light up (kept LOW), on Wifi connection it will blink,
//      when connected to the Wifi it will turn off (kept HIGH).
#define IOTWC_STATUS_PIN LED_BUILTIN

//#define MQTT_TOPIC_PREFIX ""

// -- Ignore/limit status changes more frequent than the value below (milliseconds).
#define ACTION_FEQ_LIMIT 7000
#define NO_ACTION -1

// -- Callback method declarations.
void mqttMessageReceived(String &topic, String &data);

DNSServer dnsServer;
ESP8266WebServer webServer(80);
ESP8266HTTPUpdateServer httpUpdateServer;
WiFiClient wifiClient;


IotWebConf iotWebConf(appName, &dnsServer, &webServer, wifiInitialApPassword, IOTWC_CONFIG_VERSION);

boolean needReset = false;


DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);



/* unsigned long lastMqttConnectionAttempt = 0;
int needAction = NO_ACTION;
int state = LOW;
unsigned long lastAction = 0;
char mqttActionTopic[STRING_LEN];
char mqttStatusTopic[STRING_LEN];
 */
//
// Arduino Setup
//
void setup() 
{
  bool doubleReset = false;

  Serial.begin(2000000);
  Serial << endl << appName << F("starting up...\n");

  if (drd.detectDoubleReset()) {
    Serial << F("Double reset detected\n");
    doubleReset = true;
  } else {
    Serial << F("No double reset detected. Continue normally\n");
    doubleReset = false;
  }

  if (doubleReset) {

  }

  setupIotWebConf();
  // Convert strings to numeric types
  iotWebConfConvertStringParameters();

  setupMqttClient();

//  setupNtp();

//  setupArduinoOTA();


    // -- Initializing the configuration.
  bool validConfig = iotWebConf.init();
  if (!validConfig)
  {
    Serial << F("iotWebConf did not find valid config. Initializing to defaults\n");

    mqttServer[0] = '\0';
    mqttPort[0] = '\0';
    mqttTopicPraefix[0] = '\0';
    mqttHeartbeatInterval[0] = '\0';
    mqttTimeTopic[0] = '\0';

    otaUpdatePassword[0] = '\0';

    ntpServer[0] = '\0';
    ntpTzOffset[0] = '\0';
  }

  // Reduce wait time on boot up
  iotWebConf.setApTimeoutMs(0);

  // -- Set up required URL handlers on the web server.
  server.on("/", handleRoot);
  //server.on("/boot", handleBoot);
  server.on("/config", []{ iotWebConf.handleConfig(); });
  server.onNotFound([](){ iotWebConf.handleNotFound(); });

  /* 
  Serial << F("Initialize SPIFFS...\n");
  SPIFFS.begin();
  {
    FSInfo fs_info;
    SPIFFS.info(fs_info);
    Serial << F("totalBytes:    ") << fs_info.totalBytes << endl;
    Serial << F("usedBytes:     ") << fs_info.usedBytes << endl;
    Serial << F("blockSize:     ") << fs_info.blockSize << endl;
    Serial << F("pageSize:      ") << fs_info.pageSize << endl;
    Serial << F("maxOpenFiles:  ") << fs_info.maxOpenFiles << endl;
    Serial << F("maxPathLength: ") << fs_info.maxPathLength << endl;
  }
  Serial << F("SPIFFS done.") << endl;
*/
  Serial << F("Heap: ") << system_get_free_heap_size() << endl;

  Serial << F("Setup RGB ...") << endl;
  setup_RGB();
  Serial << F("RGB done.") << endl;
}

void loop() 
{
  iotWebConf.doLoop();
  ArduinoOTA.handle();
  drd.loop();

  loopMqtt();

  if (needReset) {
    Serial << F("Reboot requested\n");
    iotWebConf.delay(1000);
    ESP.restart();
  }

  loopNtp();
 
  loop_RGB();
}

/**
 * Handle web requests to "/" path.
 */

void handleBoot()
{
  Serial << F("Reboot requested.\n");
  needReset = true;  
}






