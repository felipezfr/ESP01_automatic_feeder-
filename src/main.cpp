/**
 * SYNTAX:
 *
 * RealtimeDatabase::get(<AsyncClient>, <path>, <AsyncResultCallback>, <SSE>, <uid>);
 *
 * RealtimeDatabase::get(<AsyncClient>, <path>, <DatabaseOption>, <AsyncResultCallback>, <uid>);
 *
 * <AsyncClient> - The async client.
 * <path> - The node path to get/watch the value.
 * <DatabaseOption> - The database options (DatabaseOptions).
 * <AsyncResultCallback> - The async result callback (AsyncResultCallback).
 * <uid> - The user specified UID of async result (optional).
 * <SSE> - The Server-sent events (HTTP Streaming) mode.
 *
 * The complete usage guidelines, please visit https://github.com/mobizt/FirebaseClient
 */

#include <Arduino.h>
#if defined(ESP32) || defined(ARDUINO_RASPBERRY_PI_PICO_W) || defined(ARDUINO_GIGA) || defined(ARDUINO_OPTA)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#elif __has_include(<WiFiNINA.h>) || defined(ARDUINO_NANO_RP2040_CONNECT)
#include <WiFiNINA.h>
#elif __has_include(<WiFi101.h>)
#include <WiFi101.h>
#elif __has_include(<WiFiS3.h>) || defined(ARDUINO_UNOWIFIR4)
#include <WiFiS3.h>
#elif __has_include(<WiFiC3.h>) || defined(ARDUINO_PORTENTA_C33)
#include <WiFiC3.h>
#elif __has_include(<WiFi.h>)
#include <WiFi.h>
#endif

#include <FirebaseClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define WIFI_SSID "Zaffari 2.4GHz"
#define WIFI_PASSWORD "zaffari87"

#define DATABASE_URL "https://zaffari-automatic-feeder-dev-default-rtdb.firebaseio.com"

#define DEVICE_ID "terneiros"

struct ProductInfo
{
  String id;
  String name;
  unsigned int quantity;
  unsigned int timeInMinutes;
  bool isFeeding;
  unsigned long feedStartTime = 0;
  unsigned long currentFeedDuration = 0;
  int lastFeedingMinute = -1;
  unsigned int feedPin = 2;
};

// User Email and password that already registerd or added in your project.
// #define USER_EMAIL "USER_EMAIL"
// #define USER_PASSWORD "USER_PASSWORD"

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

// void printResult(AsyncResult &aResult);

DefaultNetwork network; // initilize with boolean parameter to enable/disable network reconnection

NoAuth noAuth;

FirebaseApp app;

#if defined(ESP32) || defined(ESP8266) || defined(ARDUINO_RASPBERRY_PI_PICO_W)
#include <WiFiClientSecure.h>
WiFiClientSecure ssl_client_app, ssl_client_products, ssl_client_update;
#elif defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_UNOWIFIR4) || defined(ARDUINO_GIGA) || defined(ARDUINO_OPTA) || defined(ARDUINO_PORTENTA_C33) || defined(ARDUINO_NANO_RP2040_CONNECT)
#include <WiFiSSLClient.h>
WiFiSSLClient ssl_client_products, ssl_client2;
#endif

using AsyncClient = AsyncClientClass;

AsyncClient aClientApp(ssl_client_app, getNetwork(network)), aClientProducts(ssl_client_products, getNetwork(network)), aClientUpdate(ssl_client_update, getNetwork(network));
void postDeviceStatus(AsyncClient &aClient);
void productsResult(AsyncResult &aResult);
void updateSyncTimeDevice(String productId, String timeStr);
void handleFeeding(ProductInfo &product);
std::vector<ProductInfo *> getProductsToFeed();
void checkIsTimeToPostDeviceStatus();
RealtimeDatabase Database;

#define FEED_PIN 2
bool isFeeding = false;
#define gramsPerSecond 100

#define CHECK_FEEDING_INTERVAL 1000

unsigned long lastPostDeviceStatusTime = 0;
unsigned long lastCheckFeedingTime = 0;
#define POST_INTERVAL 10000

AsyncResult aResult_products, aResult_app, postResult;

// std::vector<ProductInfo *> feedingQueue;

std::vector<ProductInfo> products; // Dynamic array of products

std::vector<ProductInfo *> productsToFeed;

// Get minutes since midnight
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

// void asyncCB(AsyncResult &aResult)
// {
//   printResult(aResult);
// }

void setup()
{

  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

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

  initializeApp(aClientApp, app, getAuth(noAuth), aResult_app);

  // Binding the FirebaseApp for authentication handler.
  // To unbind, use Database.resetApp();
  app.getApp<RealtimeDatabase>(Database);

  Database.url(DATABASE_URL);

  Database.setSSEFilters("get,put,patch,keep-alive,cancel,auth_revoked");

  // Initialize NTP
  timeClient.begin();
  timeClient.setTimeOffset(-10800); // UTC-3 for Brazil (in seconds)

  // The "unauthenticate" error can be occurred in this case because we don't wait
  // the app to be authenticated before connecting the stream.
  // This is ok as stream task will be reconnected automatically when the app is authenticated.
  Database.get(aClientProducts, "/products/" + String(DEVICE_ID), aResult_products, true);

  postDeviceStatus(aClientUpdate);

  pinMode(FEED_PIN, OUTPUT);
  digitalWrite(FEED_PIN, LOW);
}

void loop()
{
  // The async task handler should run inside the main loop
  // without blocking delay or bypassing with millis code blocks.
  app.loop();
  Database.loop();

  checkIsTimeToPostDeviceStatus();

  productsToFeed = getProductsToFeed();
  if (!productsToFeed.empty())
  {
    for (auto &product : productsToFeed)
    {
      if (!product->isFeeding)
      {
        Serial.printf("Size of productsToFeed: %d\n", productsToFeed.size());

        int feedTime = product->quantity / gramsPerSecond;
        Serial.printf("Starting feed for %d seconds\n", feedTime);
        digitalWrite(product->feedPin, HIGH);
        product->isFeeding = true;
        product->feedStartTime = millis();
        product->currentFeedDuration = feedTime;
        Database.set<String>(aClientUpdate, "/products/" + String(DEVICE_ID) + "/" + product->id + "/feedingTime", getCurrentTime());
        Database.set<bool>(aClientUpdate, "/products/" + String(DEVICE_ID) + "/" + product->id + "/isFeeding", true);
      }
      else
      {
        handleFeeding(*product);
      }
    }
  }
  productsResult(aResult_products);
}

void checkIsTimeToPostDeviceStatus()
{
  unsigned long currentMillis = millis();
  if (currentMillis - lastPostDeviceStatusTime >= POST_INTERVAL)
  {
    lastPostDeviceStatusTime = currentMillis;
    postDeviceStatus(aClientUpdate);
  }
}

// Add new function
std::vector<ProductInfo *> getProductsToFeed()
{
  std::vector<ProductInfo *> productsToFeed;
  unsigned long currentMillis = millis();
  if (currentMillis - lastCheckFeedingTime >= CHECK_FEEDING_INTERVAL)
  {
    lastCheckFeedingTime = currentMillis;

    // Check feeding time
    unsigned int currentMinute = getCurrentMinuteOfDay();
    for (auto &product : products)
    {
      Serial.printf("Checking: %s, timeInMinutesDB: %d, currentMinute: %d, isFeeding: %s\n",
                    product.name.c_str(),
                    product.timeInMinutes, currentMinute, product.isFeeding ? "true" : "false");
      if (product.timeInMinutes == currentMinute && !product.isFeeding &&
          product.lastFeedingMinute != currentMinute)
      {
        productsToFeed.push_back(&product);
        product.lastFeedingMinute = currentMinute;
      }
    }
    Serial.printf("Get products to feed: %d\n", productsToFeed.size());
  }
  return productsToFeed;
}

// Add new function
void handleFeeding(ProductInfo &product)
{

  if ((millis() - product.feedStartTime) >= (product.currentFeedDuration * 1000))
  {
    digitalWrite(FEED_PIN, LOW);
    product.isFeeding = false;
    Serial.println("Feeding completed: " + product.name);
    Database.set<bool>(aClientUpdate, "/products/" + String(DEVICE_ID) + "/" + product.id + "/isFeeding", false);

    productsToFeed.erase(
        std::remove_if(productsToFeed.begin(), productsToFeed.end(),
                       [&product](ProductInfo *p)
                       {
                         return p->id == product.id;
                       }),
        productsToFeed.end());
    Serial.printf("Product removed from list: %s\n", product.id.c_str());
  }
}

void productsResult(AsyncResult &aResult)
{

  if (aResult.available())
  {
    RealtimeDatabaseResult &RTDB = aResult.to<RealtimeDatabaseResult>();

    if (strcmp(RTDB.event().c_str(), "keep-alive") == 0)
    {
      Serial.println("Keep-alive, return");
      return;
    }

    if (RTDB.isStream())
    {
      // Time
      String timeStr = getCurrentTime();

      Serial.println("----------------------------");
      Firebase.printf("task: %s\n", aResult.uid().c_str());
      Firebase.printf("event: %s\n", RTDB.event().c_str());
      Firebase.printf("path: %s\n", RTDB.dataPath().c_str());
      Firebase.printf("data: %s\n", RTDB.to<const char *>());
      Firebase.printf("type: %d\n", RTDB.type());

      // Get all products
      if (strcmp(RTDB.dataPath().c_str(), "/") == 0)
      {
        StaticJsonDocument<1024> doc;
        const char *json = RTDB.to<const char *>();

        DeserializationError error = deserializeJson(doc, json);
        if (error)
        {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
          return;
        }

        // Clear previous products
        products.clear();

        // Iterate through all objects in JSON
        for (JsonPair kv : doc.as<JsonObject>())
        {
          if (kv.value().containsKey("name") &&
              kv.value().containsKey("quantity") &&
              kv.value()["name"].as<const char *>() != nullptr &&
              strlen(kv.value()["name"].as<const char *>()) > 0)
          {
            ProductInfo product;
            product.id = kv.key().c_str();
            product.name = kv.value()["name"].as<const char *>();
            product.quantity = kv.value()["quantity"];
            product.timeInMinutes = kv.value()["timeInMinutes"];
            products.push_back(product);

            updateSyncTimeDevice(product.id, timeStr);
          }
          else
          {
            Serial.printf("Invalid product - ID: %s\n", kv.key().c_str());
            continue;
          }
        }

        // Print all products for verification
        for (const auto &product : products)
        {
          Serial.printf("Product - Name: %s, Quantity: %d, Time: %d\n",
                        product.name.c_str(), product.quantity, product.timeInMinutes);
        }
      }
      else if (RTDB.dataPath() != "/") // Path is not root, meaning it's an update
      {
        String productId = RTDB.dataPath().substring(1); // Remove leading /

        // Skip if path contains additional "/"
        if (productId.indexOf('/') != -1)
        {
          return;
        }

        // Parse update data
        const char *jsonData = RTDB.to<const char *>();
        StaticJsonDocument<200> doc;
        deserializeJson(doc, jsonData);

        // Check if product exists
        bool productExists = false;
        for (auto &product : products)
        {
          if (product.id == productId)
          {
            productExists = true;

            // Check if data is null (product deleted)
            if (strcmp(jsonData, "null") == 0)
            {
              // Find and remove product
              Serial.printf("Removing product - ID: %s\n", productId.c_str());
              products.erase(std::remove_if(products.begin(), products.end(), [&](const ProductInfo &product)
                                            { return product.id == productId; }),
                             products.end());
              break;
            }

            // Update each field if present in update data
            if (doc["name"].is<const char *>())
            {
              product.name = doc["name"].as<const char *>();
              Serial.printf("Updated product %s name to %s\n",
                            product.id.c_str(), product.name.c_str());
            }
            if (doc["quantity"].is<int>())
            {
              product.quantity = doc["quantity"];
              Serial.printf("Updated product %s quantity to %d\n",
                            product.id.c_str(), product.quantity);
            }
            if (doc["timeInMinutes"].is<int>())
            {
              product.timeInMinutes = doc["timeInMinutes"];
              Serial.printf("Updated product %s timeInMinutes to %d\n",
                            product.id.c_str(), product.timeInMinutes);
            }
            updateSyncTimeDevice(product.id, timeStr);
            break;
          }
        }

        if (!productExists)
        {
          if (doc.containsKey("name") &&
              doc["name"].as<const char *>() != nullptr &&
              strlen(doc["name"].as<const char *>()) > 0)
          {

            ProductInfo newProduct;
            newProduct.id = productId;
            newProduct.name = doc["name"].as<const char *>();
            newProduct.quantity = doc["quantity"];
            newProduct.timeInMinutes = doc["timeInMinutes"];
            products.push_back(newProduct);
            Serial.printf("New product added - ID: %s\n", productId.c_str());
            updateSyncTimeDevice(newProduct.id, timeStr);
          }
          else
          {
            Serial.println("Invalid product - missing or empty name");
          }
        }
      }
    }
    else
    {
      Serial.println("----------------------------");
      Firebase.printf("task: %s, payload: %s\n", aResult.uid().c_str(), aResult.c_str());
    }
  }
}

void updateSyncTimeDevice(String productId, String timeStr)
{
  Database.set<String>(aClientUpdate, "/products/" + String(DEVICE_ID) + "/" + productId + "/syncTimeDevice", timeStr);
  Serial.println("SyncTimeDevice: " + String(DEVICE_ID) + "/" + productId + " - " + timeStr);
}

void postDeviceStatus(AsyncClient &aClient)
{

  String timeStr = getCurrentTime();

  Serial.println("Set device status: " + String(timeStr));
  Database.set<String>(aClient, "/devices/" + String(DEVICE_ID) + "/timestamp", timeStr);
}
