#include "database/database.h"
#include "configs/wifi_config.h"
#include "utils/time_utils.h"

WiFiClientSecure ssl_client_app, ssl_client_products, ssl_client_update;
DefaultNetwork network;

FirebaseApp app;
RealtimeDatabase Database;
NoAuth noAuth;
AsyncResult aResult_products, aResult_app, postResult;
AsyncClientClass aClientApp(ssl_client_app, getNetwork(network)), aClientProducts(ssl_client_products, getNetwork(network)), aClientUpdate(ssl_client_update, getNetwork(network));

void initFirebase()
{
    Firebase.printf("Firebase Client v%s\n", FIREBASE_CLIENT_VERSION);
    Serial.println("Initializing app...");

#if defined(ESP32) || defined(ESP8266)
    ssl_client_app.setInsecure();
    ssl_client_products.setInsecure();
    ssl_client_update.setInsecure();
#if defined(ESP8266)
    ssl_client_app.setBufferSizes(4096, 1024);
    ssl_client_products.setBufferSizes(4096, 1024);
    ssl_client_update.setBufferSizes(4096, 1024);

    // In case using ESP8266 without PSRAM and you want to reduce the memory usage, you can use WiFiClientSecure instead of ESP_SSLClient (see examples/RealtimeDatabase/StreamConcurentcy/StreamConcurentcy.ino)
    // with minimum receive and transmit buffer size setting as following.
    // ssl_client_products.setBufferSizes(1024, 512);
    // ssl_client2.setBufferSizes(1024, 512);
    // Note that, because the receive buffer size was set to minimum safe value, 1024, the large server response may not be able to handle.
    // The WiFiClientSecure uses 1k less memory than ESP_SSLClient.

#endif
#endif
    initializeApp(aClientApp, app, getAuth(noAuth));
    app.getApp<RealtimeDatabase>(Database);
    Database.url(DATABASE_URL);
    Database.setSSEFilters("get,put,patch,keep-alive,cancel,auth_revoked");
}

void postDeviceStatusInDatabase()
{
    String timeStr = getCurrentTime();
    Serial.println("Set device status: " + timeStr);
    Database.set<String>(aClientUpdate, "/devices/" + String(DEVICE_ID) + "/timestamp", timeStr);
}

void updateSyncTimeDevice(String productId, String timeStr)
{
    Database.set<String>(aClientUpdate, "/products/" + String(DEVICE_ID) + "/" + productId + "/syncTimeDevice", timeStr);
    Serial.println(String("SyncTimeDevice: ") + String(DEVICE_ID) + "/" + productId + " - " + timeStr);
}

void startProductsStream()
{
    Database.get(aClientProducts, "/products/" + String(DEVICE_ID), aResult_products, true);
}
