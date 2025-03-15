#pragma once

#ifdef ESP8266
    #include <ESP8266WiFi.h>
    #include <ESP8266mDNS.h>
#else
    #include <WiFi.h>
    #include <ESPmDNS.h>
#endif

#ifdef USE_ASYNC_WIFIMANAGER
    #include <ESPAsyncWiFiManager.h>
    #include <ESPAsyncWebServer.h>
    #include <DNSServer.h>
#else
    #include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager
#endif

#include <StreamString.h>
#include <ArduinoOTA.h>
#include <functional>

//#define WIFI_RECONNECT_INTERVAL 10000  // 10 seconds
#define WIFI_RECONNECT_MAX_ATTEMPTS 5  // Maximum retries before restart

class MultiStream : public Stream
{
  private:
    Stream* _serial;
    WiFiClient* _telnetClient;
    static const size_t BUFFER_SIZE = 128;
    uint8_t _buffer[BUFFER_SIZE];
    size_t _bufferIndex;

    void flushBuffer();

  public:
    MultiStream(Stream* serial, WiFiClient* telnetClient);
    
    virtual size_t write(uint8_t c) override;
    virtual size_t write(const uint8_t* buffer, size_t size) override;
    
    virtual int available() override { return _serial->available(); }
    virtual int read() override { return _serial->read(); }
    virtual int peek() override { return _serial->peek(); }
    virtual void flush() override;

    using Print::write;
};

class Networking 
{
  private:
    const char* _hostname;
    int _resetWiFiPin;
    Stream* _serial;
    WiFiServer* _telnetServer;
    WiFiClient _telnetClient;
    MultiStream* _multiStream;
    static const int TELNET_PORT = 23;
    
    #ifdef ESP8266
    static const int OTA_PORT = 8266;
    #else
    static const int OTA_PORT = 3232;
    #endif

    void setupMDNS();
    void setupOTA();
    void setupWiFiEvents();  // New method for setting up WiFi events
    
    // WiFi event tracking variables
    bool _isReconnecting;    // Flag to track if we're in the process of reconnecting
    int _reconnectAttempts;  // Counter for reconnection attempts
    unsigned long _lastReconnectAttempt; // Timestamp of last reconnect attempt
    
    std::function<void()> _onStartOTA;
    std::function<void()> _onProgressOTA;
    std::function<void()> _onEndOTA;
    std::function<void()> _onWiFiPortalStart;

    #ifdef USE_ASYNC_WIFIMANAGER
    AsyncWebServer* _webServer;
    DNSServer* _dnsServer;
    #endif

  public:
    Networking();
    ~Networking();

    Stream* begin(const char* hostname, int resetWiFiPin, HardwareSerial& serial, long serialSpeed, std::function<void()> wifiCallback = nullptr);
    void loop();
    
    // IP address methods
    IPAddress getIPAddress() const;
    String getIPAddressString() const;
    bool isConnected() const;

    void reconnectWiFi();
    void doAtStartOTA(std::function<void()> callback);
    void doAtProgressOTA(std::function<void()> callback);
    void doAtEndOTA(std::function<void()> callback);
    void doAtWiFiPortalStart(std::function<void()> callback);

    // NTP Methods
    bool ntpStart(const char* posixString, const char** ntpServers = nullptr);
    bool ntpIsValid() const;
    time_t ntpGetEpoch(const char* posixString = nullptr);
    const char* ntpGetDate(const char* posixString = nullptr);
    const char* ntpGetTime(const char* posixString = nullptr);
    const char* ntpGetDateTime(const char* posixString = nullptr);
    struct tm ntpGetTmStruct(const char* posixString = nullptr);

  private:
    const char* _posixString;
    unsigned long _lastNtpSync;
    static const unsigned long NTP_SYNC_INTERVAL = 3600000; // 1 hour in milliseconds
    
    // Static event handlers (needed for ESP8266)
    #ifdef ESP8266
    static void _onStationModeConnected(const WiFiEventStationModeConnected& event);
    static void _onStationModeDisconnected(const WiFiEventStationModeDisconnected& event);
    static void _onStationModeGotIP(const WiFiEventStationModeGotIP& event);
    
    // Static instance pointer for callbacks
    static Networking* _instance;
    #endif
};
