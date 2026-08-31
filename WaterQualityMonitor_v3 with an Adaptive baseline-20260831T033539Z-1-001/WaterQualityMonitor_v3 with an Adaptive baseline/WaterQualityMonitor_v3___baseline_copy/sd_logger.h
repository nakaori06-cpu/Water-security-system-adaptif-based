#ifndef SD_LOGGER_H
#define SD_LOGGER_H

#include <Arduino.h>

void initSDLogger();
void updateSDLogger();

bool sdCardAvailable();

#endif