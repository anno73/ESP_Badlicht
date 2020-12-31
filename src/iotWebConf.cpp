
#include <Arduino.h>

#include <string.h>
#include <Streaming.h>

#include <ESP8266HTTPUpdateServer.h>

#include "global.h"

#include "iotWebConf_.h"

#include "mqtt.h"
#include "ota.h"
#include "ntp.h"

// -- Configuration specific key. The value should be modified if config structure was changed.
#define IOTWC_CONFIG_VERSION "BADRGB_002"

// -- When BUTTON_PIN is pulled to ground on startup, the Thing will use the initial
//      password to build an AP. (E.g. in case of lost password)
#define IOTWC_BUTTON_PIN 2

// -- Status indicator pin.
//      First it will light up (kept LOW), on Wifi connection it will blink,
//      when connected to the Wifi it will turn off (kept HIGH).
#define IOTWC_STATUS_PIN LED_BUILTIN

DNSServer dnsServer;
ESP8266WebServer webServer(80);
WiFiClient wifiClient;
ESP8266HTTPUpdateServer httpUpdateServer;


// -- Initial name of the Thing. Used e.g. as SSID of the own Access Point.
const char appName[] = "BAD_RGB";

// -- Initial password to connect to the Thing, when it creates an own Access Point.
const char wifiInitialApPassword[] = "12345678";


// -- Method declarations.
void iotWebConfConvertStringParameters(void);

// Callback method declarations - find implementation after IotWebConf initialization
void wifiConnected();
void configSaved();
bool formValidator();

IotWebConf iotWebConf(appName, &dnsServer, &webServer, wifiInitialApPassword, IOTWC_CONFIG_VERSION);


IotWebConfSeparator iotMqttSeparator = IotWebConfSeparator("MQTT Parameter");
  // label, id
  // valueBuffer, length
  // type, placeholder
  // default value, custom HTML
IotWebConfParameter iotMqttServer = IotWebConfParameter(
  "MQTT Server", "mqttServer", 
  mqttServer, MQTT_SERVER_STR_LEN,
  "text", mqttServer,
  mqttServer, NULL
);
IotWebConfParameter iotMqttPort = IotWebConfParameter(
  "MQTT Port", "mqttPort", 
  mqttPort, MQTT_PORT_STR_LEN, 
  "number", "1883", 
  mqttPort, "min='1' max='65535' step='1'"
);
//IotWebConfParameter iotMqttPort = IotWebConfParameter("MQTT Port", "mqttPort", mqttPort, 5, "number", "1..65535", NULL, "min='1' max='65535' step='1'");
IotWebConfParameter iotMqttTopicPraefix = IotWebConfParameter(
  "MQTT Topic Praefix", "mqttTopicPraefix", 
  mqttTopicPraefix, MQTT_TOPIC_PRAEFIX_STR_LEN
);
IotWebConfParameter iotMqttHeartbeatInterval = IotWebConfParameter(
  "MQTT Hearbeat Interval", "mqttHeartbeatInterval",
  mqttHeartbeatInterval, MQTT_HEARTBEAT_INTERVALL_STR_LEN, 
  "number", "in millis", 
  mqttHeartbeatInterval, "min='1' max='65535' step='1'"
);
IotWebConfParameter iotMqttConnectRetryDelay = IotWebConfParameter(
  "MQTT connect retry delay", "mqttConnectRetryDelay",
  mqttConnectRetryDelay, MQTT_CONNECT_RETRY_DELAY_STR_LEN, 
  "number", "in millis", 
  mqttConnectRetryDelay, "min='1' max='65535' step='1'"
);
IotWebConfParameter iotMqttTimeTopic = IotWebConfParameter(
  "MQTT Time Topic", "mqttTimeTopic",
  mqttTimeTopic, MQTT_TIME_TOPIC_STR_LEN, 
  "text"
);

IotWebConfSeparator iotOtaSeparator = IotWebConfSeparator("OTA Parameter");
IotWebConfParameter iotOtaUpdatePassword = IotWebConfParameter(
  "OTA Update Password", "otaUpdatePassword", 
  ota::otaUpdatePassword, OTA_UPDATE_PASWORD_STR_LEN,
  "password"
);

IotWebConfSeparator iotNtpSeparator = IotWebConfSeparator("NTP");
IotWebConfParameter iotNtpServer = IotWebConfParameter(
  "NTP Server", "ntpServer", 
  ntpServer, NTP_SERVER_STR_LEN,
  "text"
);
IotWebConfParameter iotNtpTzOffset = IotWebConfParameter(
  "NTP timezone Offset", "ntpTzOffset", 
  ntpTzOffset, NTP_TZ_OFFSET_STR_LEN,
  "number"
);

//
// Called from main setup
//
void setupIotWebConf() {

  Serial << F("Setup IotWebConf") << endl;

  iotWebConf.addParameter(&iotMqttSeparator);
  iotWebConf.addParameter(&iotMqttServer);
  iotWebConf.addParameter(&iotMqttPort);
  iotWebConf.addParameter(&iotMqttTopicPraefix);
  iotWebConf.addParameter(&iotMqttConnectRetryDelay);
  iotWebConf.addParameter(&iotMqttTimeTopic);

  iotWebConf.addParameter(&iotOtaSeparator);
  iotWebConf.addParameter(&iotOtaUpdatePassword);

  iotWebConf.addParameter(&iotNtpSeparator);
  iotWebConf.addParameter(&iotNtpServer);
  iotWebConf.addParameter(&iotNtpTzOffset);
    
  
  iotWebConf.setStatusPin(IOTWC_STATUS_PIN);
  iotWebConf.setConfigPin(IOTWC_BUTTON_PIN);
  
  iotWebConf.setConfigSavedCallback(&configSaved);
  iotWebConf.setFormValidator(&formValidator);
  iotWebConf.setWifiConnectionCallback(&wifiConnected);
  iotWebConf.setupUpdateServer(&httpUpdateServer);

  // Initialize configuration
  bool validConfig = iotWebConf.init();
  if (! validConfig) {
    Serial << F("iotWebConf did not find valid config. Initializing\n");

    mqttServer[0] = 0;
    mqttPort[0] = 0;
    mqttTopicPraefix[0] = 0;
    mqttConnectRetryDelay[0] = 0;
    mqttHeartbeatInterval[0] = 0;
    mqttTimeTopic[0] = 0;

    ota::otaUpdatePassword[0] = 0;

    ntpServer[0] = 0;
    ntpTzOffset[0] = 0;
  }

  // Reduce wait time on boot up
  iotWebConf.setApTimeoutMs(0);

  // Set up required URL handlers on the web server
  webServer.on("/", handleRoot);
//  webServer.on("/boot", handleBoot);
  webServer.on("/config", [] { iotWebConf.handleConfig(); });
  webServer.onNotFound([]() { iotWebConf.handleNotFound(); });

  iotWebConfConvertStringParameters();

} // setupIotWebConv


//
// Called from main loop.
//
void loopIotWebConf()
{
    iotWebConf.doLoop();
} // loopIotWebconf

//
// Dirty hack to convert strings to integers.
//
void iotWebConfConvertStringParameters() {

  mqttPortInt = atoi(mqttPort);

  mqttConnectRetryDelayInt = atoi(mqttConnectRetryDelay);

  mqttHeartbeatIntervalInt = atoi(mqttHeartbeatInterval);

  mqttConnectRetryDelayInt = atoi(mqttConnectRetryDelay);

  mqttTopicPraefixLength = strlen(mqttTopicPraefix);

  ntpTzOffsetInt = atoi(ntpTzOffset);

} // iotWebConfConvertStringParameters

//
// Respond with root page information.
//
void handleRoot()
{
  if (iotWebConf.handleCaptivePortal()) {
    // Let IotWebConf test and handle captive portal requests.
    return;
  }

  String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
  s += "<title>";
  s += appName;
  s += "</title></head><body><h1>MQTT settings</h1>";
  s += "<ul>";
  s += "<li>MQTT server: ";
  s += mqttServer;
  s += "<li>MQTT port: ";
  s += mqttPort;
  s += "<li>MQTT Topic Praefix: ";
  s += mqttTopicPraefix;
  s += "<li>OTA Server: ";
  s += mqttTopicPraefix;
  s += "<li>NTP Server: ";
  s += mqttTopicPraefix;
  s += "<li>Timezone: ";
  s += mqttTopicPraefix;

  s += "</ul>";
  s += "Go to <a href='config'>configure page</a> to change values.";
  s += "</body></html>\n";

  webServer.send(200, "text/html", s);
} // handleRoot

//
// Request a reboot of the device.
//
void handleBoot()
{
  Serial << F("Reboot requested.\n");
  needReset = true;  
} // handleBoot

//
// Callback when WiFi is connected. Triggers some actions
//
void wifiConnected() {
  Serial << F("WiFi is connected, trigger MQTT and NTP\n");
  mqttNeedConnect = true;
  ntpNeedUpdate = true;
} // wifiConnected

//
// Callback when configuration is saved. Triggers some actions.
//
void configSaved() {
  Serial << F("Configuration saved. Reboot needed\n");
  // Reboot is the easiest way to utilize new configuration
  needReset = true;
} // configSaved

//
// ToDo: validate configuration parameters.
//
bool formValidator() {
  Serial << F("Validating form\n");

  bool valid = true;


  // check for digits in numeric values?


  return valid;
} // formValidator
