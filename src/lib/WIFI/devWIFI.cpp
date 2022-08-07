#include "device.h"

#if defined(PLATFORM_ESP8266) || defined(PLATFORM_ESP32)

#if defined(TARGET_UNIFIED_TX) || defined(TARGET_UNIFIED_RX)
#include <ArduinoJson.h>
#if defined(PLATFORM_ESP8266)
#include <FS.h>
#else
#include <SPIFFS.h>
#endif
#endif

#if defined(PLATFORM_ESP32)

#include <ESPmDNS.h>
#include <Update.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#else
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#define wifi_mode_t WiFiMode_t
#endif
#include <DNSServer.h>

#include <set>
#include <StreamString.h>

#include <ESPAsyncWebServer.h>

#include "common.h"
#include "POWERMGNT.h"
#include "hwTimer.h"
#include "logging.h"
#include "options.h"
#include "helpers.h"
#include "devVTXSPI.h"

#include "devWebUpdate.h"
#include "devTlmPassthro.h"

#include "config.h"
#if defined(TARGET_TX)
extern TxConfig config;
#else
extern RxConfig config;
#endif
extern unsigned long rebootTime;

static char station_ssid[33];
static char station_password[65];

static bool wifiStarted = false;
bool webserverPreventAutoStart = false;
extern bool InBindingMode;

static wl_status_t laststatus = WL_IDLE_STATUS;
volatile WiFiMode_t wifiMode = WIFI_OFF;
static volatile WiFiMode_t changeMode = WIFI_OFF;
static volatile unsigned long changeTime = 0;

static IPAddress netMsk(255, 255, 255, 0);
static DNSServer dnsServer;
static IPAddress ipAddress;


bool scanComplete = false;

bool isWifiStarted(void)
{
  return (bool)wifiStarted;
}

WiFiMode_t getWifiMode(void)
{
  return wifiMode;
}

void setWifiMode(WiFiMode_t mode)
{
  changeMode = mode;
}

void setWifiTimeout(unsigned long time)
{
  changeTime = time;
}

/** Is this an IP? */
static boolean isIp(String str)
{
  for (size_t i = 0; i < str.length(); i++)
  {
    int c = str.charAt(i);
    if (c != '.' && (c < '0' || c > '9'))
    {
      return false;
    }
  }
  return true;
}

/** IP to String? */
static String toStringIp(IPAddress ip)
{
  String res = "";
  for (int i = 0; i < 3; i++)
  {
    res += String((ip >> (8 * i)) & 0xFF) + ".";
  }
  res += String(((ip >> 8 * 3)) & 0xFF);
  return res;
}



static void wifiOff()
{
  wifiStarted = false;
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  #if defined(PLATFORM_ESP8266)
  WiFi.forceSleepBegin();
  #endif
}

static void startWiFi(unsigned long now)
{
  if (wifiStarted) {
    return;
  }

  if (connectionState < FAILURE_STATES) {
    hwTimer::stop();

#ifdef HAS_VTX_SPI
    VTxOutputMinimum();
#endif

    // Set transmit power to minimum
    POWERMGNT::setPower(MinPower);
    connectionState = wifiUpdate;

    DBGLN("Stopping Radio");
    Radio.End();
  }

  DBGLN("Begin Webupdater");

  WiFi.persistent(false);
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  #if defined(PLATFORM_ESP8266)
    WiFi.setOutputPower(13);
    WiFi.setPhyMode(WIFI_PHY_MODE_11N);
  #elif defined(PLATFORM_ESP32)
    WiFi.setTxPower(WIFI_POWER_13dBm);
  #endif
  strcpy(station_ssid, firmwareOptions.home_wifi_ssid);
  strcpy(station_password, firmwareOptions.home_wifi_password);
  if (station_ssid[0] == 0) {
    changeTime = now;
    changeMode = WIFI_AP;
  }
  else {
    changeTime = now;
    changeMode = WIFI_STA;
  }
  laststatus = WL_DISCONNECTED;
  wifiStarted = true;
}


static void HandleWebUpdate()
{
  unsigned long now = millis();
  wl_status_t status = WiFi.status();

  if (status != laststatus && wifiMode == WIFI_STA) {
    DBGLN("WiFi status %d", status);
    switch(status) {
      case WL_NO_SSID_AVAIL:
      case WL_CONNECT_FAILED:
      case WL_CONNECTION_LOST:
        changeTime = now;
        changeMode = WIFI_AP;
        break;
      case WL_DISCONNECTED: // try reconnection
        changeTime = now;
        break;
      default:
        break;
    }
    laststatus = status;
  }
  if (status != WL_CONNECTED && wifiMode == WIFI_STA && (now - changeTime) > 30000) {
    changeTime = now;
    changeMode = WIFI_AP;
    DBGLN("Connection failed %d", status);
  }
  if (changeMode != wifiMode && changeMode != WIFI_OFF && (now - changeTime) > 500) {
    switch(changeMode) {
      case WIFI_AP:
        DBGLN("Changing to AP mode");
        WiFi.disconnect();
        wifiMode = WIFI_AP;
        WiFi.mode(wifiMode);
        changeTime = now;
        WiFi.softAPConfig(ipAddress, ipAddress, netMsk);
        WiFi.softAP(wifi_ap_ssid, wifi_ap_password);
        WebUpdate_startService();
        break;
      case WIFI_STA:
        DBGLN("Connecting to network '%s'", station_ssid);
        wifiMode = WIFI_STA;
        WiFi.mode(wifiMode);
        WiFi.setHostname(wifi_hostname); // hostname must be set after the mode is set to STA
        changeTime = now;
        WiFi.begin(station_ssid, station_password);
        WebUpdate_startService();
      default:
        break;
    }
    #if defined(PLATFORM_ESP8266)
      MDNS.notifyAPChange();
    #endif
    changeMode = WIFI_OFF;
  }

  #if defined(PLATFORM_ESP8266)
  if (scanComplete)
  {
    WiFi.mode(wifiMode);
    scanComplete = false;
  }
  #endif

  if (WebUpdate_isServiceStarted())
  {
    dnsServer.processNextRequest();
    #if defined(PLATFORM_ESP8266)
      MDNS.update();
    #endif
    // When in STA mode, a small delay reduces power use from 90mA to 30mA when idle
    // In AP mode, it doesn't seem to make a measurable difference, but does not hurt
    if (!Update.isRunning())
      delay(1);
  }
}


static int start()
{
  ipAddress.fromString(wifi_ap_address);
  return firmwareOptions.wifi_auto_on_interval;
}

static int event()
{
  if ((connectionState == wifiUpdate) || (connectionState == tlmPassthro) || connectionState > FAILURE_STATES)
  {
    if (!wifiStarted) {
      startWiFi(millis());
      return DURATION_IMMEDIATELY;
    }
  }
  return DURATION_IGNORE;
}

static int timeout()
{
  if (wifiStarted)
  {
    if (getTlmPassStatus() == true)
    {
      HandleTlm2WIFI();
    }
    else
    {
      HandleWebUpdate();
    }
    return DURATION_IMMEDIATELY;
  }

  #if defined(TARGET_TX)
  // if webupdate was requested before or .wifi_auto_on_interval has elapsed but uart is not detected
  // start webupdate, there might be wrong configuration flashed.
  if(firmwareOptions.wifi_auto_on_interval != -1 && webserverPreventAutoStart == false && connectionState < wifiUpdate && !wifiStarted){
    DBGLN("No CRSF ever detected, starting WiFi");
    connectionState = wifiUpdate;
    return DURATION_IMMEDIATELY;
  }
  #elif defined(TARGET_RX)
  if (firmwareOptions.wifi_auto_on_interval != -1 && !webserverPreventAutoStart && (connectionState == disconnected))
  {
    static bool pastAutoInterval = false;
    // If InBindingMode then wait at least 60 seconds before going into wifi,
    // regardless of if .wifi_auto_on_interval is set to less
    if (!InBindingMode || firmwareOptions.wifi_auto_on_interval >= 60000 || pastAutoInterval)
    {
      // No need to ExitBindingMode(), the radio is about to be stopped. Need
      // to change this before the mode change event so the LED is updated
      InBindingMode = false;
      connectionState = wifiUpdate;
      return DURATION_IMMEDIATELY;
    }
    pastAutoInterval = true;
    return (60000 - firmwareOptions.wifi_auto_on_interval);
  }
  #endif
  return DURATION_NEVER;
}

device_t WIFI_device = {
  .initialize = wifiOff,
  .start = start,
  .event = event,
  .timeout = timeout
};

#endif
