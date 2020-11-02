#define DEVMODE 1

#include "JsonMapper.h"

String topic = "test/greeting";

char *wifiSSID = "POTAE";
char *wifiPassword = "aabbccdd";

const static String wsServer = "node-js-mqtt-broker.herokuapp.com";
const int wsPort = 80;
const static char* wsDomain = "/mqtt";

long lastTimeMQTTPublish = 0;

void mqttPayloadProcess(char* mqttPayload, int mqttPayloadLength) {
  Serial.print("mqttPayload: >>");
  Serial.print(mqttPayload);
  Serial.println("<<");
//  String debug = getJsonProperty(mqttPayload, "debug");
//  Serial.print("debug: ");
//  Serial.println(debug);
}

#include "MqttWebsocket.h"
void setup() {
  Serial.begin(115200);
  Serial.setTimeout(5);
  connectWifi(wifiSSID, wifiPassword);
  setWifiConnected();
  checkInternet(wifiSSID, wifiPassword);
  mqttSetup(wsServer, wsPort, 15000, (char*)wsDomain);
}

void loop() {
  if (isWifiConnected()) {
    mqttLoop();
  }
//  if (isMqttConnected()) {
//    if (millis() - lastTimeMQTTPublish > 5000) {
//      Json json;
//      json.addIntegerProperty("debug", millis());
//      publishStackPush(topic, json.getJson());
//      lastTimeMQTTPublish = millis();
//    }
//  }
}
