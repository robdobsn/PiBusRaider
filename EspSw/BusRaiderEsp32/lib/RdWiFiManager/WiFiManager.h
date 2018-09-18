// WiFi Manager
// Rob Dobson 2018

#pragma once

#include <Arduino.h>
#include <ArduinoLog.h>
#include <WiFi.h>
#include "ConfigNVS.h"
#include "StatusLed.h"
#include <ESPmDNS.h>

class WiFiManager
{
  private:
    bool _wifiEnabled;
    String _ssid;
    String _password;
    static String _hostname;
    String _defaultHostname;
    unsigned long _lastWifiBeginAttemptMs;
    bool _wifiFirstBeginDone;
    static constexpr unsigned long TIME_BETWEEN_WIFI_BEGIN_ATTEMPTS_MS = 60000;
    ConfigBase *_pConfigBase;
    static StatusLed *_pStatusLed;

  public:
    WiFiManager()
    {
        _wifiEnabled = false;
        _lastWifiBeginAttemptMs = 0;
        _wifiFirstBeginDone = false;
        _pConfigBase = NULL;
        _pStatusLed = NULL;
    }

    bool isEnabled()
    {
        return _wifiEnabled;
    }
    
    String getHostname()
    {
        return _hostname;
    }
    
    void setup(ConfigBase& hwConfig, ConfigBase *pSysConfig, const char *defaultHostname, StatusLed *pStatusLed)
    {
        _wifiEnabled = hwConfig.getLong("wifiEnabled", 0) != 0;
        _pConfigBase = pSysConfig;
        _defaultHostname = defaultHostname;
        _pStatusLed = pStatusLed;
        // Get the SSID, password and hostname if available
        _ssid = pSysConfig->getString("WiFiSSID", "");
        _password = pSysConfig->getString("WiFiPW", "");
        _hostname = pSysConfig->getString("WiFiHostname", _defaultHostname.c_str());
    }

    void service()
    {
        if (!_wifiEnabled)
            return;
        if (WiFi.status() != WL_CONNECTED)
        {
            if ((!_wifiFirstBeginDone) || (Utils::isTimeout(millis(), _lastWifiBeginAttemptMs, TIME_BETWEEN_WIFI_BEGIN_ATTEMPTS_MS)))
            {
                _wifiFirstBeginDone = true;
                WiFi.onEvent(wiFiEventHandler);
                WiFi.begin(_ssid.c_str(), _password.c_str());
                WiFi.setHostname(_hostname.c_str());
                // Set an event handler for WiFi events
                _lastWifiBeginAttemptMs = millis();
                Log.notice("WiFiManager: notConn WiFi.begin SSID %s\n", _ssid.c_str());
            }
        }
    }

    bool isConnected()
    {
        return (WiFi.status() == WL_CONNECTED);
    }

    String formConfigStr()
    {
        return "{\"WiFiSSID\":\"" + _ssid + "\",\"WiFiPW\":\"" + _password + "\",\"WiFiHostname\":\"" + _hostname + "\"}";
    }

    void setCredentials(String &ssid, String &pw, String &hostname)
    {
        _ssid = ssid;
        _password = pw;
        if (hostname.length() > 0)
        {
            _hostname = hostname;
            Log.trace("WiFiManager: hostname not set, staying with %s\n", _hostname.c_str());
        }
        if (_pConfigBase)
        {
            _pConfigBase->setConfigData(formConfigStr().c_str());
            _pConfigBase->writeConfig();
        }
        // Disconnect so re-connection with new credentials occurs
        Log.trace("WifiManager: setCredentials disconnecting to allow new connection\n");
        WiFi.disconnect();
    }

    void clearCredentials()
    {
        _ssid = "";
        _password = "";
        _hostname = _defaultHostname;
        if (_pConfigBase)
        {
            _pConfigBase->setConfigData(formConfigStr().c_str());
            _pConfigBase->writeConfig();
        }
    }

    static void wiFiEventHandler(WiFiEvent_t event)
    {
        switch (event)
        {
        case SYSTEM_EVENT_STA_GOT_IP:
            Log.notice("WiFiManager: GotIP %s\n", WiFi.localIP().toString().c_str());
            if (_pStatusLed)
                _pStatusLed->setCode(1);
            //
            // Set up mDNS responder:
            // - first argument is the domain name, in this example
            //   the fully-qualified domain name is "esp8266.local"
            // - second argument is the IP address to advertise
            //   we send our IP address on the WiFi network
            if (MDNS.begin(_hostname.c_str()))
            {
                Log.notice("WiFiManager: mDNS responder started with hostname %s\n", _hostname.c_str());
            }
            else
            {
                Log.notice("WiFiManager: mDNS responder failed to start (hostname %s)\n", _hostname.c_str());
                break;
            }
            // Add service to MDNS-SD
            MDNS.addService("http", "tcp", 80);
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            Log.notice("WiFiManager: Disconnected\n");
            WiFi.reconnect();
            if (_pStatusLed)
                _pStatusLed->setCode(0);
            break;
        default:
            // INFO: Default = do nothing
            // Log.notice("WiFiManager: unknown event %d\n", event);
            break;
        }
    }
};