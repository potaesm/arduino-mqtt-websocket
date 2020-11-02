#ifndef SimpleWifi_h
#define SimpleWifi_h
#include "Arduino.h"

extern void connectWifi(char *wifiSSID, char *wifiPassword);
extern void checkInternet(char *wifiSSID, char *wifiPassword, char *connectionTestHost = "google.com", unsigned int connectionTestPort = 80);

#endif