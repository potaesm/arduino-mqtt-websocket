#include "Arduino.h"
#include "ESP8266WiFi.h"

#define DEVMODE 1

WiFiClient wifiClient;

void connectWifi(char *wifiSSID, char *wifiPassword)
{
    WiFi.mode(WIFI_OFF);
    delay(1000);
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSSID, wifiPassword);
    byte wifiConnectCount = 0;
#if defined(DEVMODE)
    Serial.println("Connecting");
#endif
    while (WiFi.status() != WL_CONNECTED)
    {
#if defined(DEVMODE)
        Serial.print(".");
#endif
        delay(1000);
        if (wifiConnectCount > 20)
        {
            delay(5000);
            ESP.reset();
        }
    }
#if defined(DEVMODE)
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(wifiSSID);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
#endif
}

void checkInternet(char *wifiSSID, char *wifiPassword, char *connectionTestHost, unsigned int connectionTestPort)
{
    bool networkCheck = false;
    if (!wifiClient.connect(connectionTestHost, connectionTestPort))
    {
        return;
    }
    String requestHeaders = String("GET ") + "/" + " HTTP/1.1\r\n" +
                             "Host: " + connectionTestHost + "\r\n" +
                             "Accept: " + "application/json" + "\r\n" +
                             "Connection: close\r\n\r\n";
    wifiClient.print(requestHeaders);
    unsigned long apiTimeout = millis();
    while (!wifiClient.available())
    {
        if (millis() - apiTimeout > 5000)
        {
            wifiClient.stop();
            return;
        }
    }
    String respondLine;
    while (wifiClient.connected())
    {
        respondLine = wifiClient.readStringUntil('\n');
        if (respondLine == "\r")
        {
            networkCheck = true;
#if defined(DEVMODE)
            Serial.println("Internet is OK");
#endif
            break;
        }
    }
    if (!networkCheck)
    {
        WiFi.disconnect();
        delay(2500);
        WiFi.begin(wifiSSID, wifiPassword);
        byte wifiConnectCount = 0;
        while (WiFi.status() != WL_CONNECTED)
        {
            delay(1000);
            wifiConnectCount++;
            // 20 second timeout
            if (wifiConnectCount > 20)
            {
                delay(5000);
                ESP.reset();
            }
        }
    }
}