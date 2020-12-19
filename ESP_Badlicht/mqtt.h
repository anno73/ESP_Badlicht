#ifndef MY_MQTT_H
#define MY_MQTT_H

#include <MQTT.h>
#include <MQTTClient.h>

extern MQTTClient mqttClient;

extern void setupMqttClient();
extern void loopMqtt();


extern char mqttServer[];
extern char mqttPort[];

extern unsigned int mqttPortInt;
extern char mqttTopicPraefix[];
extern unsigned int mqttTopicPraefixLength;
extern char mqttConnectRetryDelay[];
extern unsigned int mqttConnectRetryDelayInt;
extern char mqttHeartbeatInterval[];
extern unsigned long mqttHeartbeatIntervalInt;
//bool mqttDisabled = true;
extern char mqttTimeTopic[];

// wifiConnected callback indicates that MQTT can now connect to the broker
extern bool mqttNeedConnect;


#endif