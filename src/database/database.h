#ifndef DATABASE_H
#define DATABASE_H

#include <WiFiClientSecure.h>

#include <FirebaseClient.h>

extern FirebaseApp app;
extern RealtimeDatabase Database;
extern AsyncResult aResult_products, aResult_app, postResult;
extern AsyncClientClass aClientApp, aClientProducts, aClientUpdate;

void initFirebase();
void postDeviceStatusInDatabase();
void updateSyncTimeDevice(String productId, String timeStr);
void startProductsStream();

#endif
