// https://github.com/256dpi/arduino-mqtt
#include <MQTT.h>
#include <MQTTClient.h>

#include <Arduino.h>
#include <Streaming.h>
#include <TimeLib.h>

#include <ESP8266WiFi.h>

// https://arduinojson.org/v6/doc/
#define ARDUINOJSON_USE_LONG_LONG 1
#include <ArduinoJson.h>


//#include "mqtt.h"
#include "global.h"
#include "ntp.h"
#include "iotWebConf.h"


/*

  Missing actions: 
    get
      -
  
    info
      heartbeat
  
    set
      intensity
      off
      on
      pause
      speed


  Implemented


*/

char mqttServer[40] = "127.0.0.1";
char mqttPort[6] = "1883";
unsigned int mqttPortInt = 0;
char mqttTopicPraefix[64] = "";
unsigned int mqttTopicPraefixLength = 0;
char mqttConnectRetryDelay[7] = "5000";
unsigned int mqttConnectRetryDelayInt = 0;
char mqttHeartbeatInterval[7] = "60000";  // set to 0 to turn off heartbeat
unsigned long mqttHeartbeatIntervalInt;
bool mqttDisabled = true;
char mqttTimeTopic[64] = "";

// wifiConnected callback indicates that MQTT can now connect to the broker
bool mqttNeedConnect = false;

MQTTClient mqttClient;

void mqttSendHeartbeat();
void mqttMessageReceived(String &, String &);

//
//
//
void setupMqttClient() {

  Serial << F("MQTT setup\n");
  
  mqttClient.begin(mqttServer, mqttPortInt, wifiClient);
  //  mqttClient.setTimeout(100);
  mqttClient.setWill("lastWill", "disconnected", true, 0);
  mqttClient.onMessage(mqttMessageReceived);

  // Subscribe to topics after successful connection

  if ((mqttPort == 0) || (mqttServer[0] == '\0') || (mqttTopicPraefix[0] == '\0')) {
    Serial << F("MQTT disabled due to missing or wrong parameters given\n");
    mqttDisabled = true;
  } else {
    mqttDisabled = false;
  }
  
} // setupMqttClient

//
//
//
bool connectMqtt() {
  static unsigned long lastConnectionAttempt = 0;   // persist across calls
  unsigned long now = millis();

  if (mqttDisabled) {
    return false;
  }
    

  if (mqttConnectRetryDelayInt > now - lastConnectionAttempt) {
    // We are not due for a connection attempt
    // Tell caller we did not connect
    return false;
  }

  Serial << F("MQTT - Trying to connect\n");

  if (!mqttClient.connect(iotWebConf.getThingName())) {
    lastConnectionAttempt = now;
    Serial << F("MQTT Connection to ") << mqttServer << ':' << mqttPortInt << F(" failed: ") << mqttClient.lastError() << ':' << mqttClient.returnCode() << F(". Will try again\n");
    return false;
  }

  Serial << F("MQTT Connected\n");
  
  // Subscribe to required topics
  String s;
  s = mqttTopicPraefix;
  s += "/#";
  mqttClient.subscribe(s);
  Serial << F("MQTT subscribe to ") << s << endl;

  if ( mqttTimeTopic[0] != 0 ) {
    s = mqttTimeTopic;
    mqttClient.subscribe(s);
    Serial << F("MQTT subscribe to ") << s << endl;
  }
  return true;
} // connectMqtt

//
//
//
void loopMqtt() {


  if (! mqttClient.loop()) {
    //      Serial << F("MQTT client.loop error: ") << mqttClient.lastError() << ':' << mqttClient.returnCode() << endl;
  }

  if ((iotWebConf.getState() == IOTWEBCONF_STATE_ONLINE) && (! mqttClient.connected())) {
    mqttNeedConnect = true;
  }

  if (mqttNeedConnect) {
    if (connectMqtt()) {
      mqttNeedConnect = false;
    }
  }

  mqttSendHeartbeat();

  
} // loopMqtt


void mqttSendHeartbeat() {

  static unsigned long mqttNextHeartbeat = 0;

  unsigned long _now = millis();

  // Send MQTT heartbeat every once in a while
  // set mqttHeartbeatIntervalInt=0 to turn off
  if (mqttClient.connected() && mqttHeartbeatIntervalInt && _now >= mqttNextHeartbeat) {
    String topic = mqttTopicPraefix;
    topic += "/info/heartbeat";

    const int jsonCapacity = JSON_OBJECT_SIZE(10);
    StaticJsonDocument<jsonCapacity> doc;

    doc["time"] = ntpClient->getFormattedTime();
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["SSID"] = WiFi.SSID();
    doc["RSSI"] = WiFi.RSSI();
    doc["MAC"] = WiFi.macAddress();
    doc["IP"] = WiFi.localIP().toString();

    String json;
    serializeJsonPretty(doc, json);
    Serial << F("MQTT send heartbeat [") << topic << F("]:\n") << json << endl;

    serializeJson(doc, json);
    bool rc = mqttClient.publish(topic, json);

    if (! rc) {
      Serial << F("MQTT publish error: ") << mqttClient.lastError() << ':' << mqttClient.returnCode() << endl;
    }
    mqttNextHeartbeat = _now + mqttHeartbeatIntervalInt;
  }
  
} // mqttSendHeartbeat

typedef struct _pack {
    char year[5];
    char month[3];
    char day[3];
    char hour[3];
    char min[3];
    char sec[3];
} timeStrPack;

typedef union _timeStr {
  char buf[20];
  timeStrPack pack;
} timeStr;

//
//
//
void mqttMessageReceived(String &topic, String &data) {
  Serial << F("MQTT message received on '") << topic << F("' with data: '") << data << F("'\n");

  if (strncmp(topic.c_str(), mqttTimeTopic, sizeof(mqttTimeTopic)) == 0 ) {
    Serial <<  F("Received Time update: ") << data << endl;
    timeStr buf;

    // "2017-01-25T21:35:18"
    //      4  3  3  3  3  3
    strncpy(buf.buf, data.c_str(), sizeof(buf.buf));
    buf.buf[4] = 0;
    buf.buf[7] = 0;
    buf.buf[10] = 0;
    buf.buf[13] = 0;
    buf.buf[16] = 0;
    buf.buf[19] = 0;
    setTime( atoi(buf.pack.hour), 
      atoi(buf.pack.min), 
      atoi(buf.pack.sec), 
      atoi(buf.pack.day), 
      atoi(buf.pack.month), 
      atoi(buf.pack.year)
    );

    timeValid = true;

    return;
  }

  topic.remove(0, mqttTopicPraefixLength);
#if 0
  if (! P) {
    Serial << F("MQTT ignore message: Parola not initialized\n");
    return;
  }


  Serial << F("MQTT action: ") << topic << endl;

  if ( strncmp(topic.c_str(), "/info/", 6) == 0) {
    // Sent by us. Gracefully ignore
    Serial << F("MQTT ignore action ") << topic << endl;
    return;
  }
  
  if ( strncmp(topic.c_str(), "/cmd/", 5) == 0) {

    topic.remove(0, 5);

    if (strcmp(topic.c_str(), "reset") == 0) {
      needReset = true;
      return;
    }

    if (strcmp(topic.c_str(), "blank") == 0) {
      parolaTextBuffer[0] = 0;
      P->displayClear(P_ZONE_TEXT);
      P->displayReset(P_ZONE_TEXT);

      return;
    }

    Serial << F("MQTT unknown cmd ") << topic << endl;
    return;
  } // cmd

  if (strncmp(topic.c_str(), "/set/", 5) == 0) {
    
    topic.remove(0, 5);
    
    if ( strcmp(topic.c_str(), "text") == 0) {
      Serial << F("MQTT send new string to display\n");

      if ( strcmp(parolaTextBuffer, data.c_str()) == 0 ) {
        Serial << F("MQTT already show string\n");
        return;
      }
      
      strncpy(parolaTextBuffer, data.c_str(), sizeof(parolaTextBuffer));
      parolaTextBuffer[sizeof(parolaTextBuffer) - 1] = 0;

      uint16_t textWidth = P->getTextWidth(P_ZONE_TEXT, (const uint8_t*) parolaTextBuffer);
      uint16_t displayWidth = parolaZoneWidth[P_ZONE_TEXT];
      parolaTextFitsToZone = textWidth <= displayWidth;

      // debug print text, brackets depending on text length and display zone width, text width, zone width
      Serial << F("MQTT set new string ")
        << ( parolaTextFitsToZone ? "[" : "]" )
        << strlen(parolaTextBuffer) << ":" << textWidth << ":" << displayWidth
        << ( parolaTextFitsToZone ? "]" : "[" )
        << " "
        << parolaTextBuffer 
        << endl;

//    Try to get length of string to display
//    Use different Effects if string fits ...

//      uint16_t tFrom = 0, tTo = 0;

//      P->getTextExtent(P_ZONE_TEXT, tFrom, tTo);
//      Serial << "Parola.getTextExtent: " << tFrom << ":" << tTo << endl;


      P->displayClear(P_ZONE_TEXT);
      Serial << F("Parola.getTextWidth: ") << textWidth << endl;

      // Select effect based on string fitting as a whole or larger than displayable
      if (parolaTextFitsToZone) {
        // Text fits on display
        P->setTextAlignment(P_ZONE_TEXT, PA_CENTER);
//        P->setTextAlignment(P_ZONE_TEXT, PA_LEFT);
//        P->setTextAlignment(P_ZONE_TEXT, PA_RIGHT);
//        P->setTextEffect(P_ZONE_TEXT, PA_PRINT, PA_NO_EFFECT);
        P->setTextEffect(P_ZONE_TEXT, PA_SCROLL_UP, PA_NO_EFFECT);
      } else {
        // Text does not fit on display
        P->setTextAlignment(P_ZONE_TEXT, PA_LEFT);
//        P->setTextEffect(P_ZONE_TEXT, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
        P->setTextEffect(P_ZONE_TEXT, PA_SCROLL_UP, PA_SCROLL_LEFT);
      }

      // Let displayAnimate in loopParola restart the animation
      P->displayReset(P_ZONE_TEXT);
      
//      P->getTextExtent(P_ZONE_TEXT, tFrom, tTo);
//      Serial << "Parola.getTextExtent: " << tFrom << ":" << tTo << endl;

      return;
    }
    
    if ( strcmp(topic.c_str(), "off") == 0) {
      Serial << F("MQTT turn display off\n");
      P->displayShutdown(true);
      return;
    }

    if ( strcmp(topic.c_str(), "on") == 0) {
      Serial << F("MQTT turn display on\n");
      P->displayShutdown(false);
      return;
    }

    if ( strcmp(topic.c_str(), "intensity") == 0) {
//      uint8_t v = strtoul(data.c_str(), NULL, 10) % 16;
      uint16_t pwmValue = strtoul(data.c_str(), NULL, 10);
      uint8_t v = (pwmValue % 100) % 16;
      pwmValue /= 100;
      pwmValue %= 1024;
    
//      Serial << F("MQTT set new intensity: ") << v << endl;
      Serial << F("MQTT set new intensity: ") << pwmValue << ":" << v << endl;
      P->setIntensity(v);
      analogWrite(PWM, pwmValue);
      return;
    }
    
    if ( strcmp(topic.c_str(), "pause") == 0) {
      uint16_t v  = strtoul(data.c_str(), NULL, 10);
      Serial << F("MQTT set new pause: ") << v << endl;
      P->setPause(P_ZONE_TEXT, v);
      return;
    }
    
    if ( strcmp(topic.c_str(), "speed") == 0) {
      uint16_t v  = strtoul(data.c_str(), NULL, 10);
      Serial << F("MQTT set new speed: ") << v << endl;
      P->setSpeed(P_ZONE_TEXT, v);
      return;
    } 
    
/*    else {
      Serial << F("MQTT unknown action ") << topic << endl;
      return;
    }
*/
    Serial << F("MQTT unknown set '") << topic << "'\n"; 
    return;
  } // set

//  if ( strncmp(topic.c_str(), "/get/", 5) == 0) {
//    Serial << F("MQTT unknown get ") << topic << endl;
//    return;
//  } // get

  {
    Serial << F("MQTT unknown topic '") << topic << "'\n";
    return;
  }

#endif

  // parse topic and react
  return;
} // mqttMessageReceived