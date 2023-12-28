#include <AsyncWiFiManager.h>

void setup()
{
    Serial.begin(115200);

    // Setup AP in portal config mode
    AsyncWiFiManager::setAPInformation("esp32 AP", "12345678");
    // Setup mDNS -> Website can be accessed via http://esp32.local
    AsyncWiFiManager::setMDnsServerName("esp32");
    // Automatically switch to config portal mode when unable to connect to saved WiFi (default: false)
    AsyncWiFiManager::setAutoConfigPortalEnable(false);

    // Begin
    AsyncWiFiManager::begin();
}

void loop()
{
    AsyncWiFiManager::loop();
}