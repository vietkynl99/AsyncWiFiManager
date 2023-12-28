#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>

enum AsyncWiFiState
{
    ASYNC_WIFI_STATE_NONE,
    ASYNC_WIFI_STATE_CONNECTING,
    ASYNC_WIFI_STATE_CONFIG_PORTAL,
    ASYNC_WIFI_STATE_CONNECTED,
    ASYNC_WIFI_STATE_DISCONNECTED
};

class AsyncWiFiManager
{
private:
    static String mSavedSSID;
    static String mSavedPassword;
    static String mAPSSID;
    static String mAPPassword;
    static int mState;
    static WebServer *mServer;
    static bool mStartedmDNS;
    static bool mIsScanning;
    static bool mIsAutoConfigPortalEnable;
    static String mMDnsServerName;
    static Preferences *mPreferences;

public:
    static void begin();
    static void loop();
    static void resetSettings();

    // Only used for debugging.The device needs to restart after using it.
    static void setWifiInformation(String ssid, String password);

    // Must call before begin()
    static void setAPInformation(String ssid, String password);
    static void setAutoConfigPortalEnable(bool enabled);
    static void setMDnsServerName(String serverName);

    static void printScannedNetWorks();
    static int getState();
    static String getStateStr();

private:
    static void setState(int state);
    static void startScanNetworks();
    static void stopScanNetworks();
    static String getEncryptionTypeStr(uint8_t encType);
    static bool getScannedWifiHtmlStr(String &str);

    static bool isValidWifiSettings();
    static void readSavedSettings();
    static void saveSettings();
    static void startConfigPortal();
    static void stopConfigPortal();
    static void startServer();
    static void stopServer();
    static void startMDNS();
    static void stopMDNS();
    static void startConnectToSavedWifi();
    static void stopConnectToSavedWifi();

    static void sendNotFound();
    static void notFoundHandler();
    static void rootHandler();
    static void saveDataHandler();

    static void processHandler();

    static void putString(const char *name, const char *key, String value);
    static String getString(const char *name, const char *key, String defaultValue = "");

    static int getRssiLevel(int rssi);
    static void trim(String &str);
};