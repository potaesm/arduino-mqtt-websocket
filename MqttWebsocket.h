#include "Arduino.h"
#include "Hash.h"
#include "WebSocketsClient.h"
#include "SimpleWifi.h"

String clientName = "ESP8266Client";

const static String MQTT_username = "MQTT_username";
const static String MQTT_password = "MQTT_password";
const static int MQTT_ALIVE_TIME = 30;

#define MQTT_PING_INTERVAL 10000
#define MQTT_CONNECT_INTERVAL 10000
#define WIFI_CHECK_INTERVAL 10000
#define BATTERY_CHECK_INTERVAL 10000

#define MQTT_TOPIC_LENGTH_MAX 64
#define MQTT_PAYLOAD_LENGTH_MAX 256

#define WIFI_FAIL 0
#define WIFI_CONNECTED 1
#define WS_DISCONNECTED 2
#define WS_CONNECTED 3
#define MQTT_CONNECTED 4

byte WS_MQTT_status = WIFI_FAIL;

WebSocketsClient webSocket;

long lastTimeMQTTActive = -10000;
long lastTimeMQTTConnect = 0;
int mqtt_feedback_count = 0;

byte MQTT_package_id = 2;
static uint8_t bufferMQTTPing[2] = {192, 0};
static uint8_t MQTTConnectHeader[12] = {16, 78, 0, 4, 77, 81, 84, 84, 4, 194, 0, 15};
static uint8_t pSubscribeRequest[MQTT_TOPIC_LENGTH_MAX + 7];
static uint8_t pUnSubscribeRequest[MQTT_TOPIC_LENGTH_MAX + 6];
static uint8_t pPublishRequest[MQTT_TOPIC_LENGTH_MAX + MQTT_PAYLOAD_LENGTH_MAX + 5];
static uint8_t pConnectRequest[128];

void mqttConnect(String mqtt_clentID, uint8_t mqtt_alive_time, String mqtt_username, String mqtt_password)
{
    MQTTConnectHeader[11] = mqtt_alive_time;
    MQTTConnectHeader[1] = 10 + 2 + mqtt_clentID.length() + 2 + mqtt_username.length() + 2 + mqtt_password.length();
    int i;
    for (i = 0; i < 12; i++)
    {
        pConnectRequest[i] = MQTTConnectHeader[i];
    }
    pConnectRequest[12] = 0;
    pConnectRequest[13] = mqtt_clentID.length();
    for (i = 0; i < mqtt_clentID.length(); i++)
    {
        pConnectRequest[i + 12 + 2] = (uint8_t)mqtt_clentID.charAt(i);
    }
    pConnectRequest[12 + 2 + mqtt_clentID.length()] = 0;
    pConnectRequest[12 + 2 + mqtt_clentID.length() + 1] = mqtt_username.length();
    for (i = 0; i < mqtt_username.length(); i++)
    {
        pConnectRequest[i + 12 + 2 + mqtt_clentID.length() + 2] = (uint8_t)mqtt_username.charAt(i);
    }
    pConnectRequest[12 + 2 + mqtt_clentID.length() + 2 + mqtt_username.length()] = 0;
    pConnectRequest[12 + 2 + mqtt_clentID.length() + 2 + mqtt_username.length() + 1] = mqtt_password.length();
    for (i = 0; i < mqtt_password.length(); i++)
    {
        pConnectRequest[i + 12 + 2 + mqtt_clentID.length() + 2 + mqtt_username.length() + 2] = (uint8_t)mqtt_password.charAt(i);
    }
    webSocket.sendBIN(pConnectRequest, MQTTConnectHeader[1] + 2);
}

byte stack_exe_used = 0;
#define STACK_EXE_DEPTH 6
String Stack_Execute[2][STACK_EXE_DEPTH];

void publishStackPush(String str_topic, String str_payload)
{
    if (WS_MQTT_status == MQTT_CONNECTED && str_topic.length() < 64 && str_payload.length() < 256)
    {
        for (byte i = STACK_EXE_DEPTH - 1; i >= 0; i--)
        {
            if (Stack_Execute[0][i].length() == 0 && Stack_Execute[1][i].length() == 0)
            {
                Stack_Execute[0][i] = str_topic;
                Stack_Execute[1][i] = str_payload;
                stack_exe_used++;
                break;
            }
        }
    }
}

void publishStackPop()
{
    if ((Stack_Execute[0][STACK_EXE_DEPTH - 1].length() > 0 && Stack_Execute[1][STACK_EXE_DEPTH - 1].length() > 0))
    {
#if defined(DEVMODE)
        Serial.print("Topic: ");
        Serial.println(Stack_Execute[0][STACK_EXE_DEPTH - 1].c_str());
        Serial.print("Payload: ");
        Serial.println(Stack_Execute[1][STACK_EXE_DEPTH - 1].c_str());
#endif
        if (Stack_Execute[1][STACK_EXE_DEPTH - 1] == "SUBSCRIBE")
        {
            pSubscribeRequest[0] = 130;
            pSubscribeRequest[1] = 5 + Stack_Execute[0][STACK_EXE_DEPTH - 1].length();
            pSubscribeRequest[2] = 0;
            pSubscribeRequest[3] = MQTT_package_id;
            pSubscribeRequest[4] = 0;
            pSubscribeRequest[5] = Stack_Execute[0][STACK_EXE_DEPTH - 1].length();
            for (int i = 0; i < Stack_Execute[0][STACK_EXE_DEPTH - 1].length(); i++)
            {
                pSubscribeRequest[i + 6] = (uint8_t)Stack_Execute[0][STACK_EXE_DEPTH - 1].charAt(i);
            }
            pSubscribeRequest[Stack_Execute[0][STACK_EXE_DEPTH - 1].length() + 6] = 0; // QOS=0
            webSocket.sendBIN(pSubscribeRequest, Stack_Execute[0][STACK_EXE_DEPTH - 1].length() + 6 + 1);
        }
        else if (Stack_Execute[1][STACK_EXE_DEPTH - 1] == "UNSUBSCRIBE")
        {

            pUnSubscribeRequest[0] = 162;
            pUnSubscribeRequest[1] = 4 + Stack_Execute[0][STACK_EXE_DEPTH - 1].length();
            pUnSubscribeRequest[2] = 0;
            pUnSubscribeRequest[3] = MQTT_package_id;
            pUnSubscribeRequest[4] = 0;
            pUnSubscribeRequest[5] = Stack_Execute[0][STACK_EXE_DEPTH - 1].length();
            for (int i = 0; i < Stack_Execute[0][STACK_EXE_DEPTH - 1].length(); i++)
            {
                pUnSubscribeRequest[i + 6] = (uint8_t)Stack_Execute[0][STACK_EXE_DEPTH - 1].charAt(i);
            }
            webSocket.sendBIN(pUnSubscribeRequest, Stack_Execute[0][STACK_EXE_DEPTH - 1].length() + 6);
            MQTT_package_id++;
        }
        else
        {
            if (Stack_Execute[0][STACK_EXE_DEPTH - 1].length() + Stack_Execute[1][STACK_EXE_DEPTH - 1].length() + 2 > 127)
            {
                pPublishRequest[0] = 48;
                pPublishRequest[1] = (uint8_t)(Stack_Execute[0][STACK_EXE_DEPTH - 1].length() + Stack_Execute[1][STACK_EXE_DEPTH - 1].length() + 2) - ((Stack_Execute[0][STACK_EXE_DEPTH - 1].length() + Stack_Execute[1][STACK_EXE_DEPTH - 1].length() + 2) / 128 - 1) * 128;
                pPublishRequest[2] = (uint8_t)(Stack_Execute[0][STACK_EXE_DEPTH - 1].length() + Stack_Execute[1][STACK_EXE_DEPTH - 1].length() + 2) / 128;
                pPublishRequest[3] = 0;
                pPublishRequest[4] = (uint8_t)Stack_Execute[0][STACK_EXE_DEPTH - 1].length();
                for (int i = 0; i < Stack_Execute[0][STACK_EXE_DEPTH - 1].length(); i++)
                {
                    pPublishRequest[i + 5] = (uint8_t)Stack_Execute[0][STACK_EXE_DEPTH - 1].charAt(i);
                }
                for (int i = 0; i < Stack_Execute[1][STACK_EXE_DEPTH - 1].length(); i++)
                {
                    pPublishRequest[i + 5 + Stack_Execute[0][STACK_EXE_DEPTH - 1].length()] = (uint8_t)Stack_Execute[1][STACK_EXE_DEPTH - 1].charAt(i);
                }
                webSocket.sendBIN(pPublishRequest, Stack_Execute[0][STACK_EXE_DEPTH - 1].length() + Stack_Execute[1][STACK_EXE_DEPTH - 1].length() + 5);
            }
            else
            {
                pPublishRequest[0] = 48;
                pPublishRequest[1] = (uint8_t)Stack_Execute[0][STACK_EXE_DEPTH - 1].length() + Stack_Execute[1][STACK_EXE_DEPTH - 1].length() + 2;
                pPublishRequest[2] = 0;
                pPublishRequest[3] = (uint8_t)Stack_Execute[0][STACK_EXE_DEPTH - 1].length();
                for (int i = 0; i < Stack_Execute[0][STACK_EXE_DEPTH - 1].length(); i++)
                {
                    pPublishRequest[i + 4] = (uint8_t)Stack_Execute[0][STACK_EXE_DEPTH - 1].charAt(i);
                }
                for (int i = 0; i < Stack_Execute[1][STACK_EXE_DEPTH - 1].length(); i++)
                {
                    pPublishRequest[i + 4 + Stack_Execute[0][STACK_EXE_DEPTH - 1].length()] = (uint8_t)Stack_Execute[1][STACK_EXE_DEPTH - 1].charAt(i);
                }
                webSocket.sendBIN(pPublishRequest, Stack_Execute[0][STACK_EXE_DEPTH - 1].length() + Stack_Execute[1][STACK_EXE_DEPTH - 1].length() + 4);
            }
        }
        for (byte i = STACK_EXE_DEPTH - 1; i > 0; i--)
        {
            Stack_Execute[0][i] = Stack_Execute[0][i - 1];
            Stack_Execute[1][i] = Stack_Execute[1][i - 1];
        }
        Stack_Execute[0][0] = "";
        Stack_Execute[1][0] = "";
        stack_exe_used--;
    }
}

// mqttPayloadProcess(char* mqttPayload, int mqttPayloadLength)
// {
// #if defined(DEVMODE)
//     Serial.print("mqttPayload: >>");
//     Serial.print(mqttPayload);
//     Serial.println("<<");
// #endif
// }

void mqttCallback(uint8_t *MSG_payload, size_t MSG_length)
{
    if (WS_MQTT_status == MQTT_CONNECTED)
    {
        if (MSG_length == 2)
        {
            if (MSG_payload[0] == 208 && MSG_payload[1] == 0)
            {
                mqtt_feedback_count = 0;
                // Serial.println("MQTT ping response");
            }
        }
        if (MSG_length > 4)
        {
            if (MSG_payload[0] == 48)
            {
                if (MSG_length > 127 + 2)
                {
                    if ((MSG_payload[2] - 1) * 128 + MSG_payload[1] + 3 == MSG_length)
                    {
                        char mqtt_payload[MSG_length - 5 - MSG_payload[4]];
                        for (int i = 5 + MSG_payload[4]; i < MSG_length; i++)
                        {
                            mqtt_payload[i - 5 - MSG_payload[4]] = (char)MSG_payload[i];
                        }
                        mqttPayloadProcess(mqtt_payload, MSG_length - 5 - MSG_payload[4]);
                    }
                }
                else if (MSG_payload[1] + 2 == MSG_length)
                {
                    char mqtt_payload[MSG_length - 4 - MSG_payload[3]];
                    for (int i = 4 + MSG_payload[3]; i < MSG_length; i++)
                    {
                        mqtt_payload[i - 4 - MSG_payload[3]] = (char)MSG_payload[i];
                    }
                    mqttPayloadProcess(mqtt_payload, MSG_length - 4 - MSG_payload[3]);
                }
            }
        }
    }
    else if (WS_MQTT_status == WS_CONNECTED)
    {
        if (MSG_length == 4)
        {
            if (MSG_payload[0] == 32 && MSG_payload[3] == 0)
            {
                WS_MQTT_status = MQTT_CONNECTED;
                publishStackPush(topic, "SUBSCRIBE");
#if defined(DEVMODE)
                Serial.println("MQTT connected");
#endif
            }
        }
    }
}

void mqttLoop()
{
    webSocket.loop();
    if (WS_MQTT_status == MQTT_CONNECTED)
    {
        publishStackPop();
        if (millis() - lastTimeMQTTActive > MQTT_PING_INTERVAL)
        {
            if (mqtt_feedback_count > 2)
            {
                WS_MQTT_status = WS_DISCONNECTED;
                webSocket.disconnect();
                mqtt_feedback_count = 0;
                lastTimeMQTTConnect = millis();
            }
            else
            {
                webSocket.sendBIN(bufferMQTTPing, 2);
                // Serial.print("MQTT ping request with feedback count ");
                // Serial.println(mqtt_feedback_count);
                mqtt_feedback_count++;
            }
            lastTimeMQTTActive = millis();
        }
    }
    else if (WS_MQTT_status == WS_CONNECTED)
    {
        if ((millis() - lastTimeMQTTConnect) > MQTT_CONNECT_INTERVAL)
        {
            mqttConnect(clientName, MQTT_ALIVE_TIME, MQTT_username, MQTT_password);
            lastTimeMQTTConnect = millis();
#if defined(DEVMODE)
            Serial.println("Connected to the broker as " + clientName);
#endif
        }
    }
}

void wsCallbackEvent(WStype_t MSG_type, uint8_t *MSG_payload, size_t MSG_length)
{
    switch (MSG_type)
    {
    case WStype_DISCONNECTED:
    {
        WS_MQTT_status = WS_DISCONNECTED;
#if defined(DEVMODE)
        Serial.println("Websocket disconnected");
#endif
        checkInternet(wifiSSID, wifiPassword);
        break;
    }
    case WStype_CONNECTED:
    {
        WS_MQTT_status = WS_CONNECTED;
#if defined(DEVMODE)
        Serial.println("Websocket connected");
#endif
        break;
    }
    case WStype_TEXT:
        break;
    case WStype_BIN:
    {
        mqttCallback(MSG_payload, MSG_length);
        break;
    }
    }
}

void mqttSetup(String mqtt_server, int mqtt_port, int mqtt_reconnect_interval, char *mqtt_domain)
{
    webSocket.setReconnectInterval(mqtt_reconnect_interval);
    webSocket.begin(mqtt_server, mqtt_port, mqtt_domain);
    webSocket.onEvent(wsCallbackEvent);
    WS_MQTT_status = WS_DISCONNECTED;
}

void setWifiConnected()
{
    WS_MQTT_status = WIFI_CONNECTED;
}

bool isWifiConnected()
{
    return WS_MQTT_status >= WS_DISCONNECTED;
}

bool isMqttConnected()
{
    return WS_MQTT_status == MQTT_CONNECTED;
}