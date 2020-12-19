#ifndef MY_MQTT_H
#define MY_MQTT_H

#include <MQTT.h>
#include <MQTTClient.h>

extern MQTTClient mqttClient;

extern void setupMqttClient();
extern void loopMqtt();


#define MQTT_SERVER_STR_LEN 40
extern char mqttServer[];

#define MQTT_PORT_STR_LEN 6
extern char mqttPort[];
extern unsigned int mqttPortInt;

#define MQTT_TOPIC_PRAEFIX_STR_LEN 64
extern char mqttTopicPraefix[];
extern unsigned int mqttTopicPraefixLength;

#define MQTT_CONNECT_RETRY_DELAY_STR_LEN 7
extern char mqttConnectRetryDelay[];
extern unsigned int mqttConnectRetryDelayInt;

#define MQTT_HEARTBEAT_INTERVALL_STR_LEN 7
extern char mqttHeartbeatInterval[];
extern unsigned long mqttHeartbeatIntervalInt;

//bool mqttDisabled = true;
#define MQTT_TIME_TOPIC_STR_LEN 64
extern char mqttTimeTopic[];

// wifiConnected callback indicates that MQTT can now connect to the broker
extern bool mqttNeedConnect;


#endif