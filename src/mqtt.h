#pragma once

#include <MQTT.h>
#include <MQTTClient.h>

extern MQTTClient mqttClient;

extern void setupMqttClient();
extern void loopMqtt();


const uint8_t MQTT_SERVER_STR_LEN = 40;
extern char mqttServer[];

const uint8_t MQTT_PORT_STR_LEN = 6;
extern char mqttPort[];
extern uint16_t mqttPortInt;

const uint8_t MQTT_TOPIC_PRAEFIX_STR_LEN = 64;
extern char mqttTopicPraefix[];
extern uint16_t mqttTopicPraefixLength;

const uint8_t MQTT_CONNECT_RETRY_DELAY_STR_LEN = 7;
extern char mqttConnectRetryDelay[];
extern uint16_t mqttConnectRetryDelayInt;

const uint8_t MQTT_HEARTBEAT_INTERVALL_STR_LEN = 7;
extern char mqttHeartbeatInterval[];
extern unsigned long mqttHeartbeatIntervalInt;

//bool mqttDisabled = true;
const uint8_t MQTT_TIME_TOPIC_STR_LEN = 64;
extern char mqttTimeTopic[];

// wifiConnected callback indicates that MQTT can now connect to the broker
extern bool mqttNeedConnect;
