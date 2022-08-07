#pragma once

#include "device.h"
#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
#include <WiFi.h>

extern device_t WIFI_device;

WiFiMode_t getWifiMode(void);
void setWifiMode(WiFiMode_t mode);
void setWifiMode(WiFiMode_t mode);
void setWifiTimeout(unsigned long time);

#define HAS_WIFI
#endif