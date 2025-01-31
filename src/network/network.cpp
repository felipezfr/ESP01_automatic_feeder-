#include "network/network.h"
#include "configs/wifi_config.h"

void connectToWiFi()
{
    Serial.print("Connecting to Wi-Fi");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(300);
    }
    Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
}