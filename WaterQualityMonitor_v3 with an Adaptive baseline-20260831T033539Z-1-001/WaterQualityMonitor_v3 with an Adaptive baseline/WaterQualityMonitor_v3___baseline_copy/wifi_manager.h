#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

void initWiFi();
void updateWiFi();

bool isWiFiConnected();

String getWiFiSSID();
String getWiFiIP();

unsigned long getWiFiConnectedTime();

#endif