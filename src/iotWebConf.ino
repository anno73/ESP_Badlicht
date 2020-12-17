
#include "iotWebConf.h"

// Callback method declarations - find implementation after IotWebConf initialization
void wifiConnected();
void configSaved();
bool formValidator();

IotWebConfSeparator iotMqttSeparator = IotWebConfSeparator("MQTT Parameter");
  // label, id
  // valueBuffer, length
  // type, placeholder
  // default value, custom HTML
IotWebConfParameter iotMqttServer = IotWebConfParameter(
  "MQTT Server", "mqttServer", 
  mqttServer, sizeof(mqttServer),
  "text", mqttServer,
  mqttServer, NULL
);
IotWebConfParameter iotMqttPort = IotWebConfParameter(
  "MQTT Port", "mqttPort", 
  mqttPort, sizeof(mqttPort), 
  "text", mqttPort
);
//IotWebConfParameter iotMqttPort = IotWebConfParameter("MQTT Port", "mqttPort", mqttPort, 5, "number", "1..65535", NULL, "min='1' max='65535' step='1'");
IotWebConfParameter iotMqttTopicPraefix = IotWebConfParameter(
  "MQTT Topic Praefix", "mqttTopicPraefix", 
  mqttTopicPraefix, sizeof(mqttTopicPraefix)
);
IotWebConfParameter iotMqttHeartbeatInterval = IotWebConfParameter(
  "MQTT Hearbeat Interval", "mqttHeartbeatInterval",
  mqttHeartbeatInterval, sizeof(mqttHeartbeatInterval), 
  "number", "in millis", 
  mqttHeartbeatInterval, "min='1' max='65535' step='1'"
);
IotWebConfParameter iotMqttTimeTopic = IotWebConfParameter(
  "MQTT Time Topic", "mqttTimeTopic",
  mqttTimeTopic, sizeof(mqttTimeTopic), 
  "text"
);

IotWebConfSeparator iotOtaSeparator = IotWebConfSeparator("OTA Parameter");
IotWebConfParameter iotOtaUpdatePassword = IotWebConfParameter(
  "OTA Update Password", "otaUpdatePassword", 
  otaUpdatePassword, sizeof(otaUpdatePassword),
  "password"
);


/* IotWebConfSeparator iotParolaSeparator = IotWebConfSeparator("Parola Parameter");
IotWebConfParameter iotParolaCountModules = IotWebConfParameter(
  "MAX72xx Modules count", "parolaCountModules", 
  parolaCountModules, sizeof(parolaCountModules),
  "number"
); */

IotWebConfSeparator iotNtpSeparator = IotWebConfSeparator("NTP");
IotWebConfParameter iotNtpServer = IotWebConfParameter(
  "NTP Server", "ntpServer", 
  ntpServer, sizeof(ntpServer),
  "text"
);
IotWebConfParameter iotNtpTzOffset = IotWebConfParameter(
  "NTP timezone Offset", "ntpTzOffset", 
  ntpTzOffset, sizeof(ntpTzOffset),
  "number"
);


//
//
//
void setupIotWebConf() {

  Serial << F("IotWebConf setup\n");

  iotWebConf.addParameter(&iotMqttSeparator);
  iotWebConf.addParameter(&iotMqttServer);
  iotWebConf.addParameter(&iotMqttPort);
  iotWebConf.addParameter(&iotMqttTopicPraefix);
  iotWebConf.addParameter(&iotMqttTimeTopic);

  iotWebConf.addParameter(&iotOtaSeparator);
  iotWebConf.addParameter(&iotOtaUpdatePassword);

//  iotWebConf.addParameter(&iotParolaSeparator);
//  iotWebConf.addParameter(&iotParolaCountModules);

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
  boolean validConfig = iotWebConf.init();
  if (! validConfig) {
    Serial << F("iotWebConf did not find valid config. Initializing\n");

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

  // Set up required URL handlers on the web server
  webServer.on("/", handleRoot);
  webServer.on("/config", [] { iotWebConf.handleConfig(); });
  webServer.onNotFound([]() { iotWebConf.handleNotFound(); });

} // setupIotWebConv

//
//
//
void iotWebConfConvertStringParameters() {

  mqttPortInt = atoi(mqttPort);

  mqttConnectRetryDelayInt = atoi(mqttConnectRetryDelay);

  mqttHeartbeatIntervalInt = atoi(mqttHeartbeatInterval);

//  parolaCountModulesInt = atoi(parolaCountModules);
//  parolaCountModulesInt = 12;

  mqttTopicPraefixLength = strlen(mqttTopicPraefix);

  ntpTzOffsetInt = atoi(ntpTzOffset);

} // iotWebConfConvertStringParameters

//
//
//
void handleRoot()
{
  if (iotWebConf.handleCaptivePortal()) {
    // Let IotWebConf test and handle captive portal reqeusts.
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
//
//
void wifiConnected() {
  Serial << F("WiFi is connected, trigger MQTT and NTP\n");
  mqttNeedConnect = true;
  ntpNeedUpdate = true;
} // wifiConnected

void configSaved() {
  Serial << F("Configuration saved. Reboot needed\n");
  // Reboot is the easiest way to utilize new configuration
  needReset = true;
}

//
//
//
bool formValidator() {
  Serial << F("Validating form\n");

  boolean valid = true;


  // check for digits in numeric values?


  return valid;
} // formValidator
