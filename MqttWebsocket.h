#include "Arduino.h"
#include "Hash.h"
#include "WebSocketsClient.h"
#include "SimpleWifi.h"

#define DEVMODE 1

char* mqttClientId = "ESPClient";
char* mqttUsername = "MQTT_USERNAME";
char* mqttPassword = "MQTT_PASSWORD";
uint8_t mqttAliveTime = 30;

String mqttTopic = "esp/topic";

#define MQTT_PING_INTERVAL 10000
#define MQTT_CONNECT_INTERVAL 10000

#define MQTT_TOPIC_LENGTH_MAX 64
#define MQTT_PAYLOAD_LENGTH_MAX 256

#define WIFI_FAIL 0
#define WIFI_CONNECTED 1
#define WS_DISCONNECTED 2
#define WS_CONNECTED 3
#define MQTT_CONNECTED 4

byte WS_MQTT_STATUS = WIFI_FAIL;

WebSocketsClient webSocket;

long lastTimeMQTTActive = -10000;
long lastTimeMQTTConnect = 0;
int mqttFeedbackCount = 0;

byte mqttPackageId = 2;
static uint8_t bufferMQTTPing[2] = {192, 0};
static uint8_t mqttConnectHeader[12] = {16, 78, 0, 4, 77, 81, 84, 84, 4, 194, 0, 15};
static uint8_t pSubscribeRequest[MQTT_TOPIC_LENGTH_MAX + 7];
static uint8_t pUnSubscribeRequest[MQTT_TOPIC_LENGTH_MAX + 6];
static uint8_t pPublishRequest[MQTT_TOPIC_LENGTH_MAX + MQTT_PAYLOAD_LENGTH_MAX + 5];
static uint8_t pConnectRequest[128];

void mqttConnect(char* mqttClientId, uint8_t mqttAliveTime, char* mqttUsername, char* mqttPassword)
{
    uint8_t mqttClientIdLength = strlen(mqttClientId);
    uint8_t mqttUsernameLength = strlen(mqttUsername);
    uint8_t mqttPasswordLength = strlen(mqttPassword);
    mqttConnectHeader[11] = mqttAliveTime;
    mqttConnectHeader[1] = 10 + 2 + mqttClientIdLength + 2 + mqttUsernameLength + 2 + mqttPasswordLength;
    int i;
    for (i = 0; i < 12; i++)
    {
        pConnectRequest[i] = mqttConnectHeader[i];
    }
    pConnectRequest[12] = 0;
    pConnectRequest[13] = mqttClientIdLength;
    for (i = 0; i < mqttClientIdLength; i++)
    {
        pConnectRequest[i + 12 + 2] = (uint8_t) mqttClientId[i];
    }
    pConnectRequest[12 + 2 + mqttClientIdLength] = 0;
    pConnectRequest[12 + 2 + mqttClientIdLength + 1] = mqttUsernameLength;
    for (i = 0; i < mqttUsernameLength; i++)
    {
        pConnectRequest[i + 12 + 2 + mqttClientIdLength + 2] = (uint8_t) mqttUsername[i];
    }
    pConnectRequest[12 + 2 + mqttClientIdLength + 2 + mqttUsernameLength] = 0;
    pConnectRequest[12 + 2 + mqttClientIdLength + 2 + mqttUsernameLength + 1] = mqttPasswordLength;
    for (i = 0; i < mqttPasswordLength; i++)
    {
        pConnectRequest[i + 12 + 2 + mqttClientIdLength + 2 + mqttUsernameLength + 2] = (uint8_t) mqttPassword[i];
    }
    webSocket.sendBIN(pConnectRequest, mqttConnectHeader[1] + 2);
}

byte stackExeUsed = 0;
#define STACK_EXE_DEPTH 6
String stackExecute[2][STACK_EXE_DEPTH];

void publishStackPush(String topicString, String payloadString)
{
    if (WS_MQTT_STATUS == MQTT_CONNECTED && topicString.length() < 64 && payloadString.length() < 256)
    {
        for (byte i = STACK_EXE_DEPTH - 1; i >= 0; i--)
        {
            if (stackExecute[0][i].length() == 0 && stackExecute[1][i].length() == 0)
            {
                stackExecute[0][i] = topicString;
                stackExecute[1][i] = payloadString;
                stackExeUsed++;
                break;
            }
        }
    }
}

void publishStackPop()
{
    if ((stackExecute[0][STACK_EXE_DEPTH - 1].length() > 0 && stackExecute[1][STACK_EXE_DEPTH - 1].length() > 0))
    {
#if defined(DEVMODE)
        Serial.print("Topic: ");
        Serial.println(stackExecute[0][STACK_EXE_DEPTH - 1].c_str());
        Serial.print("Payload: ");
        Serial.println(stackExecute[1][STACK_EXE_DEPTH - 1].c_str());
#endif
        if (stackExecute[1][STACK_EXE_DEPTH - 1] == "SUBSCRIBE")
        {
            pSubscribeRequest[0] = 130;
            pSubscribeRequest[1] = 5 + stackExecute[0][STACK_EXE_DEPTH - 1].length();
            pSubscribeRequest[2] = 0;
            pSubscribeRequest[3] = mqttPackageId;
            pSubscribeRequest[4] = 0;
            pSubscribeRequest[5] = stackExecute[0][STACK_EXE_DEPTH - 1].length();
            for (int i = 0; i < stackExecute[0][STACK_EXE_DEPTH - 1].length(); i++)
            {
                pSubscribeRequest[i + 6] = (uint8_t)stackExecute[0][STACK_EXE_DEPTH - 1].charAt(i);
            }
            pSubscribeRequest[stackExecute[0][STACK_EXE_DEPTH - 1].length() + 6] = 0; // QOS=0
            webSocket.sendBIN(pSubscribeRequest, stackExecute[0][STACK_EXE_DEPTH - 1].length() + 6 + 1);
        }
        else if (stackExecute[1][STACK_EXE_DEPTH - 1] == "UNSUBSCRIBE")
        {

            pUnSubscribeRequest[0] = 162;
            pUnSubscribeRequest[1] = 4 + stackExecute[0][STACK_EXE_DEPTH - 1].length();
            pUnSubscribeRequest[2] = 0;
            pUnSubscribeRequest[3] = mqttPackageId;
            pUnSubscribeRequest[4] = 0;
            pUnSubscribeRequest[5] = stackExecute[0][STACK_EXE_DEPTH - 1].length();
            for (int i = 0; i < stackExecute[0][STACK_EXE_DEPTH - 1].length(); i++)
            {
                pUnSubscribeRequest[i + 6] = (uint8_t)stackExecute[0][STACK_EXE_DEPTH - 1].charAt(i);
            }
            webSocket.sendBIN(pUnSubscribeRequest, stackExecute[0][STACK_EXE_DEPTH - 1].length() + 6);
            mqttPackageId++;
        }
        else
        {
            if (stackExecute[0][STACK_EXE_DEPTH - 1].length() + stackExecute[1][STACK_EXE_DEPTH - 1].length() + 2 > 127)
            {
                pPublishRequest[0] = 48;
                pPublishRequest[1] = (uint8_t)(stackExecute[0][STACK_EXE_DEPTH - 1].length() + stackExecute[1][STACK_EXE_DEPTH - 1].length() + 2) - ((stackExecute[0][STACK_EXE_DEPTH - 1].length() + stackExecute[1][STACK_EXE_DEPTH - 1].length() + 2) / 128 - 1) * 128;
                pPublishRequest[2] = (uint8_t)(stackExecute[0][STACK_EXE_DEPTH - 1].length() + stackExecute[1][STACK_EXE_DEPTH - 1].length() + 2) / 128;
                pPublishRequest[3] = 0;
                pPublishRequest[4] = (uint8_t)stackExecute[0][STACK_EXE_DEPTH - 1].length();
                for (int i = 0; i < stackExecute[0][STACK_EXE_DEPTH - 1].length(); i++)
                {
                    pPublishRequest[i + 5] = (uint8_t)stackExecute[0][STACK_EXE_DEPTH - 1].charAt(i);
                }
                for (int i = 0; i < stackExecute[1][STACK_EXE_DEPTH - 1].length(); i++)
                {
                    pPublishRequest[i + 5 + stackExecute[0][STACK_EXE_DEPTH - 1].length()] = (uint8_t)stackExecute[1][STACK_EXE_DEPTH - 1].charAt(i);
                }
                webSocket.sendBIN(pPublishRequest, stackExecute[0][STACK_EXE_DEPTH - 1].length() + stackExecute[1][STACK_EXE_DEPTH - 1].length() + 5);
            }
            else
            {
                pPublishRequest[0] = 48;
                pPublishRequest[1] = (uint8_t)stackExecute[0][STACK_EXE_DEPTH - 1].length() + stackExecute[1][STACK_EXE_DEPTH - 1].length() + 2;
                pPublishRequest[2] = 0;
                pPublishRequest[3] = (uint8_t)stackExecute[0][STACK_EXE_DEPTH - 1].length();
                for (int i = 0; i < stackExecute[0][STACK_EXE_DEPTH - 1].length(); i++)
                {
                    pPublishRequest[i + 4] = (uint8_t)stackExecute[0][STACK_EXE_DEPTH - 1].charAt(i);
                }
                for (int i = 0; i < stackExecute[1][STACK_EXE_DEPTH - 1].length(); i++)
                {
                    pPublishRequest[i + 4 + stackExecute[0][STACK_EXE_DEPTH - 1].length()] = (uint8_t)stackExecute[1][STACK_EXE_DEPTH - 1].charAt(i);
                }
                webSocket.sendBIN(pPublishRequest, stackExecute[0][STACK_EXE_DEPTH - 1].length() + stackExecute[1][STACK_EXE_DEPTH - 1].length() + 4);
            }
        }
        for (byte i = STACK_EXE_DEPTH - 1; i > 0; i--)
        {
            stackExecute[0][i] = stackExecute[0][i - 1];
            stackExecute[1][i] = stackExecute[1][i - 1];
        }
        stackExecute[0][0] = "";
        stackExecute[1][0] = "";
        stackExeUsed--;
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

void mqttCallback(uint8_t* messagePayload, size_t messageLength)
{
    if (WS_MQTT_STATUS == MQTT_CONNECTED)
    {
        if (messageLength == 2)
        {
            if (messagePayload[0] == 208 && messagePayload[1] == 0)
            {
                mqttFeedbackCount = 0;
                // Serial.println("MQTT ping response");
            }
        }
        if (messageLength > 4)
        {
            if (messagePayload[0] == 48)
            {
                if (messageLength > 127 + 2)
                {
                    if ((messagePayload[2] - 1) * 128 + messagePayload[1] + 3 == messageLength)
                    {
                        char mqttPayload[messageLength - 5 - messagePayload[4]];
                        for (int i = 5 + messagePayload[4]; i < messageLength; i++)
                        {
                            mqttPayload[i - 5 - messagePayload[4]] = (char)messagePayload[i];
                        }
                        mqttPayloadProcess(mqttPayload, messageLength - 5 - messagePayload[4]);
                    }
                }
                else if (messagePayload[1] + 2 == messageLength)
                {
                    char mqttPayload[messageLength - 4 - messagePayload[3]];
                    for (int i = 4 + messagePayload[3]; i < messageLength; i++)
                    {
                        mqttPayload[i - 4 - messagePayload[3]] = (char)messagePayload[i];
                    }
                    mqttPayloadProcess(mqttPayload, messageLength - 4 - messagePayload[3]);
                }
            }
        }
    }
    else if (WS_MQTT_STATUS == WS_CONNECTED)
    {
        if (messageLength == 4)
        {
            if (messagePayload[0] == 32 && messagePayload[3] == 0)
            {
                WS_MQTT_STATUS = MQTT_CONNECTED;
                publishStackPush(mqttTopic, "SUBSCRIBE");
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
    if (WS_MQTT_STATUS == MQTT_CONNECTED)
    {
        publishStackPop();
        if (millis() - lastTimeMQTTActive > MQTT_PING_INTERVAL)
        {
            if (mqttFeedbackCount > 2)
            {
                WS_MQTT_STATUS = WS_DISCONNECTED;
                webSocket.disconnect();
                mqttFeedbackCount = 0;
                lastTimeMQTTConnect = millis();
            }
            else
            {
                webSocket.sendBIN(bufferMQTTPing, 2);
                // Serial.print("MQTT ping request with feedback count ");
                // Serial.println(mqttFeedbackCount);
                mqttFeedbackCount++;
            }
            lastTimeMQTTActive = millis();
        }
    }
    else if (WS_MQTT_STATUS == WS_CONNECTED)
    {
        if ((millis() - lastTimeMQTTConnect) > MQTT_CONNECT_INTERVAL)
        {
            mqttConnect(mqttClientId, mqttAliveTime, mqttUsername, mqttPassword);
            lastTimeMQTTConnect = millis();
#if defined(DEVMODE)
            Serial.println("Connecting to the broker as " + String(mqttClientId));
#endif
        }
    }
}

void wsCallbackEvent(WStype_t messageType, uint8_t* messagePayload, size_t messageLength)
{
    switch (messageType)
    {
    case WStype_DISCONNECTED:
    {
        WS_MQTT_STATUS = WS_DISCONNECTED;
#if defined(DEVMODE)
        Serial.println("Websocket disconnected");
#endif
        checkInternet(wifiSSID, wifiPassword);
        break;
    }
    case WStype_CONNECTED:
    {
        WS_MQTT_STATUS = WS_CONNECTED;
#if defined(DEVMODE)
        Serial.println("Websocket connected");
#endif
        break;
    }
    case WStype_TEXT:
        break;
    case WStype_BIN:
    {
        mqttCallback(messagePayload, messageLength);
        break;
    }
    default:
    {
        break;
    }
    }
}

void mqttSetup(char* mqttServer, int mqttPort, int mqttReconnectInterval, char* mqttDomain)
{
    webSocket.setReconnectInterval(mqttReconnectInterval);
    webSocket.begin(mqttServer, mqttPort, mqttDomain);
    webSocket.onEvent(wsCallbackEvent);
    WS_MQTT_STATUS = WS_DISCONNECTED;
}

void setMqttTopic(String topic)
{
    mqttTopic = topic;
}

void setMqttAuthentication(char* username, char* password)
{
    mqttUsername = username;
    mqttPassword = password;
}

void setMqttClientId(char* clientId)
{
    mqttClientId = clientId;
}

void setMqttWifiConnected()
{
    WS_MQTT_STATUS = WIFI_CONNECTED;
}

bool isMqttWifiConnected()
{
    return WS_MQTT_STATUS >= WS_DISCONNECTED;
}

bool isMqttConnected()
{
    return WS_MQTT_STATUS == MQTT_CONNECTED;
}