#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <Arduino.h>

void initTime();
unsigned int getCurrentMinuteOfDay();
String getCurrentTime();

#endif