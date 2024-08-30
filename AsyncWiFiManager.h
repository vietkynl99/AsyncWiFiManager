#pragma once

#include <Arduino.h>
#include <LittleFS.h>

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#define WebServerClass ESP8266WebServer
#else
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <ESPmDNS.h>

#define WebServerClass WebServer
#endif

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
    static unsigned long mConnectWifiTimeout;
    static unsigned long mConfigPortalTimeout;
    static String mSavedSSID;
    static String mSavedPassword;
    static String mAPSSID;
    static String mAPPassword;
    static int mState;
    static WebServerClass *mServer;
    static bool mStartedmDNS;
    static bool mIsScanning;
    static bool mIsAutoConfigPortalEnable;
    static String mMDnsServerName;
    static void (*onStateChanged)(AsyncWiFiState state);
    static void (*mOnWiFiInformationChanged)();

public:
    static void begin();
    static void loop();
    static void resetSettings();
    static void turnOff();

    // Only used for debugging.The device needs to restart after using it.
    static void setWifiInformation(String ssid, String password);

    // Must call before begin()
    static void setAPInformation(String ssid, String password);
    static void setAutoConfigPortalEnable(bool enabled);
    static void setMDnsServerName(String serverName);
    static void setConnectWifiTimeout(unsigned int timeout);
    static void setConfigPortalTimeout(unsigned int timeout);

    static void setOnStateChanged(void (*callback)(AsyncWiFiState state));
    static void setOnWiFiInformationChanged(void (*callback)());

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

    static int getRssiLevel(int rssi);
    static void trim(String &str);

    static void initFS();
    static bool readFile(const char *path, String &content);
    static bool writeFile(const char *path, String &content);
};