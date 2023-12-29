#include "AsyncWiFiManager.h"
#include "html/HtmlResource.h"

String AsyncWiFiManager::mSavedSSID = "";
String AsyncWiFiManager::mSavedPassword = "";
String AsyncWiFiManager::mAPSSID = "";
String AsyncWiFiManager::mAPPassword = "";
int AsyncWiFiManager::mState = ASYNC_WIFI_STATE_NONE;
WebServerClass *AsyncWiFiManager::mServer = nullptr;
bool AsyncWiFiManager::mStartedmDNS = false;
bool AsyncWiFiManager::mIsAutoConfigPortalEnable = false;
String AsyncWiFiManager::mMDnsServerName = "";
void (*AsyncWiFiManager::onStateChanged)(AsyncWiFiState state) = nullptr;
void (*AsyncWiFiManager::mOnWiFiInformationChanged)() = nullptr;

const char HTML_WIFI_ITEM1[] PROGMEM = "<div><a href='#p' onclick='c(this)'>";
const char HTML_WIFI_ITEM2[] PROGMEM = "</a><div class='q q-";
const char HTML_WIFI_LOCK[] PROGMEM = " l";
const char HTML_WIFI_ITEM3[] PROGMEM = "'></div></div>";
const char HTML_WIFI_LIST[] PROGMEM = "<!-- HTML_WIFI_LIST -->";
const char HTML_NO_NETWORKS_FOUND[] PROGMEM = "<label>No networks found</label><br>";

#define DEBUG_ENABLE_LOG
// #define DEBUG_HTTP_ARGUMENTS

#ifdef DEBUG_ENABLE_LOG
#define TAG "WIFI"
#define LOG(...)                       \
    {                                  \
        Serial.print(F("[" TAG "] ")); \
        Serial.printf(__VA_ARGS__);    \
        Serial.println();              \
    }
#define LOGE(...)               \
    {                           \
        Serial.print(F("[E]")); \
        LOG(__VA_ARGS__);       \
    }
#else
#define LOG(...)
#define LOGE(...)
#endif

#define FS LittleFS
#define FORMAT_FS_IF_FAILED true

#define CONNECT_WIFI_TIMEOUT 30000UL   // (ms)
#define CONFIG_PORTAL_TIMEOUT 120000UL // (ms)

#define AP_SSID_DEFAULT "ESP AP"
#define AP_PASSWORD_DEFAULT "12345678"
#define AP_IP_ADDR IPAddress(192, 168, 4, 1)

bool AsyncWiFiManager::mIsScanning = false;

void AsyncWiFiManager::begin()
{
    initFS();
    setState(ASYNC_WIFI_STATE_NONE);
    readSavedSettings();
    if (!isValidWifiSettings())
    {
        LOG("No saved WiFi");
        startConfigPortal();
    }
    else
    {
        startConnectToSavedWifi();
    }
}

void AsyncWiFiManager::loop()
{
    if (mServer)
    {
        mServer->handleClient();
    }
#ifdef ESP8266
    if (mStartedmDNS)
    {
        MDNS.update();
    }
#endif
    processHandler();
}

void AsyncWiFiManager::resetSettings()
{
    LOG("Reset WiFi settings");
    setWifiInformation("", "");
    // ESP.restart();
}

// This function should not be used unless debugging. The device needs to restart after using it.
void AsyncWiFiManager::setWifiInformation(String ssid, String password)
{
    mSavedSSID = ssid;
    mSavedPassword = password;
    saveSettings();
}

void AsyncWiFiManager::setAPInformation(String ssid, String password)
{
    mAPSSID = ssid;
    mAPPassword = password;
}

// Automatically switch to config portal mode when unable to connect to saved WiFi (default: false)
void AsyncWiFiManager::setAutoConfigPortalEnable(bool enabled)
{
    mIsAutoConfigPortalEnable = enabled;
}

void AsyncWiFiManager::setMDnsServerName(String serverName)
{
    mMDnsServerName = serverName;
}

void AsyncWiFiManager::setOnStateChanged(void (*callback)(AsyncWiFiState state))
{
    onStateChanged = callback;
}

void AsyncWiFiManager::setOnWiFiInformationChanged(void (*callback)())
{
    mOnWiFiInformationChanged = callback;
}

int AsyncWiFiManager::getState()
{
    return mState;
}

String AsyncWiFiManager::getStateStr()
{
    switch (mState)
    {
    case ASYNC_WIFI_STATE_NONE:
        return "NONE";
    case ASYNC_WIFI_STATE_CONNECTING:
        return "CONNECTING";
    case ASYNC_WIFI_STATE_CONFIG_PORTAL:
        return "CONFIG_PORTAL";
    case ASYNC_WIFI_STATE_CONNECTED:
        return "CONNECTED";
    case ASYNC_WIFI_STATE_DISCONNECTED:
        return "DISCONNECTED";
    default:
        return "UNKNOWN";
    }
}

void AsyncWiFiManager::setState(int state)
{
    if (mState != state)
    {
        mState = state;
        LOG("State changed to %s", getStateStr().c_str());
        if (onStateChanged)
        {
            onStateChanged((AsyncWiFiState)state);
        }
    }
}

void AsyncWiFiManager::startScanNetworks()
{
    if (!mIsScanning)
    {
        LOG("Start scan networks");
        mIsScanning = true;
        if (WiFi.status() == WL_CONNECTED)
        {
            LOG("Disconnecting networks");
            WiFi.disconnect();
        }
        if (WiFi.getMode() == WIFI_AP)
        {
            WiFi.mode(WIFI_AP_STA);
        }
        else
        {
            WiFi.mode(WIFI_STA);
        }
        WiFi.scanNetworks(true);
    }
}

void AsyncWiFiManager::stopScanNetworks()
{
    if (mIsScanning)
    {
        LOG("Stop scan networks");
        WiFi.scanDelete();
        mIsScanning = false;
    }
}

String AsyncWiFiManager::getEncryptionTypeStr(uint8_t encType)
{
#ifdef ESP8266
    switch (encType)
    {
    case AUTH_OPEN:
        return "open";
    case AUTH_WEP:
        return "WEP";
    case AUTH_WPA_PSK:
        return "WPA";
    case AUTH_WPA2_PSK:
        return "WPA2";
    case AUTH_WPA_WPA2_PSK:
        return "WPA+WPA2";
    default:
        return "unknown";
    }
#else
    switch (encType)
    {
    case WIFI_AUTH_OPEN:
        return "open";
    case WIFI_AUTH_WEP:
        return "WEP";
    case WIFI_AUTH_WPA_PSK:
        return "WPA";
    case WIFI_AUTH_WPA2_PSK:
        return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:
        return "WPA+WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE:
        return "WPA2-EAP";
    case WIFI_AUTH_WPA3_PSK:
        return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:
        return "WPA2+WPA3";
    case WIFI_AUTH_WAPI_PSK:
        return "WAPI";
    default:
        return "unknown";
    }
#endif
}

void AsyncWiFiManager::printScannedNetWorks()
{
    uint8_t i = 0;
    String ssid;
    uint8_t encType;
    int32_t rssi;
    uint8_t *bssid;
    int32_t channel;
    bool hidden = false;

    int n = WiFi.scanComplete();

#ifdef ESP8266
    while (i < n && WiFi.getNetworkInfo(i, ssid, encType, rssi, bssid, channel, hidden))
#else
    while (i < n && WiFi.getNetworkInfo(i, ssid, encType, rssi, bssid, channel))
#endif
    {
        if (hidden)
        {
            continue;
        }
        LOG("%2d. %-24s %4ddBm | %s", i + 1, ssid.c_str(), rssi, getEncryptionTypeStr(encType).c_str());
        i++;
    }
}

bool AsyncWiFiManager::getScannedWifiHtmlStr(String &str)
{
    int i = 0;
    String ssid;
    uint8_t encType;
    int32_t rssi;
    uint8_t *bssid;
    int32_t channel;
    bool hidden = false;
    bool ret = false;

    int n = WiFi.scanComplete();

#ifdef ESP8266
    while (i < n && WiFi.getNetworkInfo(i, ssid, encType, rssi, bssid, channel, hidden))
#else
    while (i < n && WiFi.getNetworkInfo(i, ssid, encType, rssi, bssid, channel))
#endif
    {
        if (hidden)
        {
            continue;
        }
        LOG("%2d. %-24s %4ddBm | %s", i + 1, ssid.c_str(), rssi, getEncryptionTypeStr(encType).c_str());

        str += FPSTR(HTML_WIFI_ITEM1);
        str += ssid;
        str += FPSTR(HTML_WIFI_ITEM2);
        str += String(getRssiLevel(rssi));
#ifdef ESP8266
        if (encType != AUTH_OPEN)
#else
        if (encType != WIFI_AUTH_OPEN)
#endif
        {
            str += FPSTR(HTML_WIFI_LOCK);
        }
        str += FPSTR(HTML_WIFI_ITEM3);
        str += "\n";
        i++;
        ret = true;
    }
    return ret;
}

bool AsyncWiFiManager::isValidWifiSettings()
{
    return mSavedSSID.length() > 0 && mSavedPassword.length() > 0;
}

void AsyncWiFiManager::readSavedSettings()
{
    if (!readFile("/ssid.txt", mSavedSSID) || !readFile("/pass.txt", mSavedPassword))
    {
        LOGE("Failed to read settings");
        mSavedSSID = "";
        mSavedPassword = "";
    }
}

void AsyncWiFiManager::saveSettings()
{
    if (!writeFile("/ssid.txt", mSavedSSID) || !writeFile("/pass.txt", mSavedPassword))
    {
        LOGE("Failed to save settings");
    }
    else if (mSavedSSID.length() > 0)
    {
        LOG("Saved WiFi: %s", mSavedSSID.c_str());
    }
}

void AsyncWiFiManager::startConfigPortal()
{
    if (mState != ASYNC_WIFI_STATE_NONE)
    {
        LOGE("Cannot start config portal");
        return;
    }

    setState(ASYNC_WIFI_STATE_CONFIG_PORTAL);
    WiFi.mode(WIFI_AP);
    if (mAPSSID.length() == 0 || mAPPassword.length() == 0)
    {
        LOG("AP SSID or password has not been set. Use default AP: '%s' '%s'", mAPSSID.c_str(), mAPPassword.c_str());
        mAPSSID = AP_SSID_DEFAULT;
        mAPPassword = AP_PASSWORD_DEFAULT;
    }
    if (!WiFi.softAP(mAPSSID, mAPPassword))
    {
        LOGE("Failed to start AP");
        setState(ASYNC_WIFI_STATE_NONE);
        return;
    }
    WiFi.softAPConfig(AP_IP_ADDR, IPAddress(0, 0, 0, 0), IPAddress(255, 255, 255, 0));
    LOG("Start config portal AP: %s", mAPSSID.c_str());

    startServer();
    startMDNS();
    startScanNetworks();
}

void AsyncWiFiManager::stopConfigPortal()
{
    if (mState != ASYNC_WIFI_STATE_CONFIG_PORTAL)
    {
        return;
    }
    LOG("Stop config portal");
    setState(ASYNC_WIFI_STATE_DISCONNECTED);
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    stopServer();
    // stopMDNS();
    stopScanNetworks();
}

void AsyncWiFiManager::startServer()
{
    if (!mServer)
    {
        LOG("Start server");
        mServer = new WebServerClass(80);
        mServer->onNotFound(notFoundHandler);
        mServer->on("/", rootHandler);
        mServer->on("/save", saveDataHandler);
        mServer->begin();
    }
}

void AsyncWiFiManager::stopServer()
{
    if (mServer)
    {
        LOG("Stop server");
        mServer->stop();
        delete mServer;
        mServer = nullptr;
    }
}

void AsyncWiFiManager::startMDNS()
{
    if (!mStartedmDNS && mMDnsServerName.length() > 0 && MDNS.begin(mMDnsServerName))
    {
        mStartedmDNS = true;
#ifdef ESP8266
        MDNS.addService("http", "tcp", 80);
#endif
        LOG("mDNS responder started at http://%s.local", mMDnsServerName.c_str());
    }
}

void AsyncWiFiManager::stopMDNS()
{
    if (mStartedmDNS)
    {
        mStartedmDNS = false;
        LOG("Stop mDNS responder");
        MDNS.end();
    }
}

void AsyncWiFiManager::startConnectToSavedWifi()
{
    if (mState != ASYNC_WIFI_STATE_NONE)
    {
        LOGE("Cannot connnect to saved wifi");
        return;
    }
    setState(ASYNC_WIFI_STATE_CONNECTING);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(mSavedSSID.c_str(), mSavedPassword.c_str());
    LOG("Connecting to %s", mSavedSSID.c_str());
    startMDNS();
}

void AsyncWiFiManager::stopConnectToSavedWifi()
{
    if (mState != ASYNC_WIFI_STATE_CONNECTING)
    {
        return;
    }
    LOG("Stop connect to saved Wifi");
    setState(ASYNC_WIFI_STATE_NONE);
    WiFi.disconnect(true);
    stopServer();
    // stopMDNS();
}

void AsyncWiFiManager::sendNotFound()
{
    if (!mServer)
    {
        return;
    }
    mServer->send(404, "text/plain", "404 Not Found");
}

void AsyncWiFiManager::notFoundHandler()
{
    if (!mServer)
    {
        return;
    }
#ifdef DEBUG_HTTP_ARGUMENTS
    String message = "URI: ";
    message += mServer->uri();
    message += "\nMethod: ";
    message += (mServer->method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += mServer->args();
    message += "\n";
    for (uint8_t i = 0; i < mServer->args(); i++)
    {
        message += " " + mServer->argName(i) + ": " + mServer->arg(i) + "\n";
    }
    LOG("Http: %s", message.c_str());
#endif
    sendNotFound();
}

void AsyncWiFiManager::rootHandler()
{
    if (!mServer)
    {
        return;
    }
#ifdef DEBUG_HTTP_ARGUMENTS
    String message = "URI: ";
    message += mServer->uri();
    message += "\nMethod: ";
    message += (mServer->method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += mServer->args();
    message += "\n";
    for (uint8_t i = 0; i < mServer->args(); i++)
    {
        message += " " + mServer->argName(i) + ": " + mServer->arg(i) + "\n";
    }
    LOG("Http: %s", message.c_str());
#endif

    String html = FPSTR(HTML_CONFIG_WIFI);
    String wifiList = "";
    if (getScannedWifiHtmlStr(wifiList))
    {
        html.replace(FPSTR(HTML_WIFI_LIST), wifiList);
    }
    else
    {
        html.replace(FPSTR(HTML_WIFI_LIST), FPSTR(HTML_NO_NETWORKS_FOUND));
    }

    mServer->send(200, "text/html", html);
}

void AsyncWiFiManager::saveDataHandler()
{
    if (!mServer)
    {
        return;
    }
    if (mServer->hasArg("s"))
    {
        mSavedSSID = mServer->arg("s");
    }
    if (mServer->hasArg("p"))
    {
        mSavedPassword = mServer->arg("p");
    }
    trim(mSavedSSID);
    trim(mSavedPassword);
    if (isValidWifiSettings())
    {
        String html = FPSTR(HTML_CONFIG_SUCCESS);
        mServer->send(200, "text/html", html);

        saveSettings();
        if (mOnWiFiInformationChanged)
        {
            mOnWiFiInformationChanged();
        }
        else
        {
            delay(1000);
            ESP.restart();
        }
    }
    else
    {
        mServer->send(200, "text/plain", F("WiFi information is invalid. Please try again."));
    }
}

void AsyncWiFiManager::processHandler()
{
    static int prevState = ASYNC_WIFI_STATE_NONE;
    static bool prevConnectStatus = false;
    static int prevWifiCount = -99;
    static unsigned long long timeoutTime = 0, scanTime = 0;

    if (prevConnectStatus != WiFi.isConnected())
    {
        prevConnectStatus = WiFi.isConnected();
        if (mState == ASYNC_WIFI_STATE_CONNECTED && !WiFi.isConnected())
        {
            setState(ASYNC_WIFI_STATE_DISCONNECTED);
        }
        else if (mState == ASYNC_WIFI_STATE_DISCONNECTED && WiFi.isConnected())
        {
            setState(ASYNC_WIFI_STATE_CONNECTED);
        }
    }

    if (mState == ASYNC_WIFI_STATE_CONNECTING && WiFi.isConnected())
    {
        setState(ASYNC_WIFI_STATE_CONNECTED);
        stopScanNetworks();
        stopServer();
    }

    if (prevState != mState)
    {
        prevState = mState;
        if (mState == ASYNC_WIFI_STATE_CONFIG_PORTAL)
        {
            timeoutTime = millis() + CONFIG_PORTAL_TIMEOUT;
        }
        else if (mState == ASYNC_WIFI_STATE_CONNECTING && mIsAutoConfigPortalEnable)
        {
            timeoutTime = millis() + CONNECT_WIFI_TIMEOUT;
        }
    }

    if (timeoutTime && !WiFi.isConnected() && millis() > timeoutTime)
    {
        timeoutTime = 0;
        if (mState == ASYNC_WIFI_STATE_CONFIG_PORTAL)
        {
            LOG("Config portal timeout");
            stopConfigPortal();
        }
        else if (mState == ASYNC_WIFI_STATE_CONNECTING)
        {
            LOG("Connect to saved Wifi timeout");
            stopConnectToSavedWifi();
            startConfigPortal();
        }
    }

    if (mIsScanning && millis() > scanTime)
    {
        scanTime = millis() + 100;
        int wifiCount = WiFi.scanComplete();
        if (prevWifiCount != wifiCount)
        {
            prevWifiCount = wifiCount;
            if (wifiCount == 0)
            {
                LOG("No networks found");
            }
            else if (wifiCount > 0)
            {
                LOG("Found %d networks", wifiCount);
            }
            else if (wifiCount == WIFI_SCAN_RUNNING)
            {
                LOG("WiFi scan running");
            }
            else
            {
                LOG("WiFi scan disabled or failed");
            }
        }
    }
}

void AsyncWiFiManager::trim(String &str)
{
    while (str.length() > 0 && (str.indexOf(' ') == 0 || str.indexOf('\r') == 0 || str.indexOf('\n') == 0))
    {
        str = str.substring(1);
    }
    while (str.length() > 0 && (str.lastIndexOf(' ') == str.length() - 1 || str.lastIndexOf('\r') == str.length() - 1 || str.lastIndexOf('\n') == str.length() - 1))
    {
        str = str.substring(0, str.length() - 1);
    }
}

int AsyncWiFiManager::getRssiLevel(int rssi)
{
    int level = 0;
    if (rssi < -90)
    {
        level = 0;
    }
    else if (rssi > -30)
    {
        level = 4;
    }
    else
    {
        level = map(rssi, -90, -30, 0, 4);
    }
    return level;
}

void AsyncWiFiManager::initFS()
{
#ifdef ESP8266
    if (!FS.begin())
    {
        LOGE("FS Mount Failed");
    }
#else
    if (!FS.begin(FORMAT_FS_IF_FAILED))
    {
        LOGE("FS Mount Failed");
    }
#endif
}

bool AsyncWiFiManager::readFile(const char *path, String &content)
{
    fs::File file = FS.open(path, "r");
    if (!file || file.isDirectory())
    {
        LOGE("Failed to open file %s for reading", path);
        return false;
    }
    content = file.readString();
    file.close();
    return true;
}

bool AsyncWiFiManager::writeFile(const char *path, String &content)
{
    fs::File file = FS.open(path, "w");
    if (!file)
    {
        LOGE("Failed to open file %s for writing", path);
        return false;
    }
    bool ret = false;
    if (file.print(content))
    {
        ret = true;
    }
    else
    {
        LOGE("Write to %s failed", path);
    }
    file.close();
    return ret;
}