#ifndef NETWORK_H
#define NETWORK_H

#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

void connectToWiFi();

#endif
