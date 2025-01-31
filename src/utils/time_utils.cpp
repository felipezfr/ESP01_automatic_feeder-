#include "utils/time_utils.h"
#include <NTPClient.h>
#include <WiFiUdp.h>

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

void initTime()
{
    timeClient.begin();
    timeClient.setTimeOffset(-10800); // UTC-3 (Brasil)
}

unsigned int getCurrentMinuteOfDay()
{
    timeClient.update();
    time_t epochTime = timeClient.getEpochTime();
    struct tm *ptm = gmtime((time_t *)&epochTime);
    return ptm->tm_hour * 60 + ptm->tm_min;
}

String getCurrentTime()
{
    timeClient.update();
    time_t epochTime = timeClient.getEpochTime();
    struct tm *ptm = gmtime((time_t *)&epochTime);
    char timeStr[20];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", ptm);
    return timeStr;
}
