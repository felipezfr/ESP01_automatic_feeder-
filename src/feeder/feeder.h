#ifndef FEEDER_H
#define FEEDER_H

#include <vector>
#include <Arduino.h>
#include "configs/feed_config.h"
#include "database/database.h"

struct ProductInfo
{
    String id;
    String name;
    unsigned int quantity;
    unsigned int timeInMinutes;
    bool isFeeding;
    unsigned long feedStartTime = 0;
    unsigned long currentFeedDuration = 0;
    unsigned int lastFeedingMinute = -1;
    unsigned int feedPin = FEED_PIN;
};
extern std::vector<ProductInfo> products;

void executeFeedingRoutine();
void handleFeeding(ProductInfo &product);
void productsResult(AsyncResult &aResult);
void postDeviceStatus();

#endif
