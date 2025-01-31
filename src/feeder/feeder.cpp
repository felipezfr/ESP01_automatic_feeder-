#include "feeder/feeder.h"
#include "utils/time_utils.h"
#include "database/database.h"
#include "configs/wifi_config.h"
#include "configs/feed_config.h"
#include <ArduinoJson.h>

std::vector<ProductInfo> products;
std::vector<ProductInfo *> productsToFeed;
unsigned long lastCheckFeedingTime = 0;
unsigned long lastPostDeviceStatusTime = 0;

std::vector<ProductInfo *> getProductsToFeed();

void executeFeedingRoutine()
{
    productsToFeed = getProductsToFeed();
    if (!productsToFeed.empty())
    {
        for (auto &product : productsToFeed)
        {
            if (!product->isFeeding)
            {
                Serial.printf("Size of productsToFeed: %d\n", productsToFeed.size());

                int feedTime = product->quantity / GRAMS_PER_SECOND;
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

std::vector<ProductInfo *> getProductsToFeed()
{
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
        Database.set<String>(aClientUpdate, "/products/" + String(DEVICE_ID) + "/" + product.id + "/endFeedingTime", getCurrentTime());
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

void postDeviceStatus()
{
    unsigned long currentMillis = millis();
    if (currentMillis - lastPostDeviceStatusTime >= POST_INTERVAL)
    {
        lastPostDeviceStatusTime = currentMillis;
        postDeviceStatusInDatabase();
    }
}