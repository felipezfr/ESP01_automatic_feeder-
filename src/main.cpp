#include <Arduino.h>
#include "network/network.h"
#include "database/database.h"
#include "feeder/feeder.h"
#include "utils/time_utils.h"

void setup()
{
  Serial.begin(115200);
  connectToWiFi();
  initFirebase();
  initTime();
  pinMode(FEED_PIN, OUTPUT);
  digitalWrite(FEED_PIN, LOW);
  startProductsStream();
}

void loop()
{
  app.loop();
  Database.loop();

  postDeviceStatus();
  executeFeedingRoutine();
}
