#ifndef MY_MQTT_H
#define MY_MQTT_H

// https://github.com/256dpi/arduino-mqtt
#include <MQTT.h>


MQTTClient mqttClient;

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
boolean mqttNeedConnect = false;


#endif