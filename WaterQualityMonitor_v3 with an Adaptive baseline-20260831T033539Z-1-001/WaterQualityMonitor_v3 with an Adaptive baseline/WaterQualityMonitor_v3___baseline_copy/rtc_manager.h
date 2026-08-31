#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#include <Arduino.h>

void initRTC();
void updateRTC();

bool rtcAvailable();

String getDateString();
String getTimeString();

uint32_t getRTCEpoch();

#endif