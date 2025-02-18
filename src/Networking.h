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
    #include <WiFiManager.h>         // https://github.com/tzapu/WiFiManager
#endif
#include <StreamString.h>
#include <ArduinoOTA.h>
#include <functional>

class MultiStream : public Stream 
{
  private:
    Stream* _serial;
    WiFiClient* _telnetClient;

  public:
    /**
     * MultiStream is a wrapper for a Stream object that writes to both a local
     * serial interface and a remote telnet client.
     *
     * @param serial The local serial interface to write to.
     * @param telnetClient The remote telnet client to write to.
     */
    MultiStream(Stream* serial, WiFiClient* telnetClient) 
        : _serial(serial), _telnetClient(telnetClient) {}

    /**
     * Writes a byte to both the serial and telnet client streams.
     *
     * @param c The byte to be written.
     * @return The total number of bytes written to both streams.
     */

    /**
     * Writes a byte to both the serial and telnet client streams.
     *
     * @param c The byte to be written.
     * @return The total number of bytes written to both streams.
     */
    virtual size_t write(uint8_t c) override 
    {
        size_t written = _serial->write(c);
        if (_telnetClient && _telnetClient->connected())
        {
            written += _telnetClient->write(c);
        }
        return written;
    }

    virtual int available() override { return _serial->available(); }
    virtual int read() override { return _serial->read(); }
    virtual int peek() override { return _serial->peek(); }
    virtual void flush() override { _serial->flush(); }

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

    void doAtStartOTA(std::function<void()> callback);
    void doAtProgressOTA(std::function<void()> callback);
    void doAtEndOTA(std::function<void()> callback);
    void doAtWiFiPortalStart(std::function<void()> callback);

    // NTP Methods
    bool ntpStart(const char* posixString, const char** ntpServers = nullptr);
    bool ntpIsValid() const;
    time_t ntpGetEpoch(const char* posixString = nullptr);
    const char* ntpGetData(const char* posixString = nullptr);
    const char* ntpGetTime(const char* posixString = nullptr);
    const char* ntpGetDateTime(const char* posixString = nullptr);
    struct tm ntpGetTmStruct(const char* posixString = nullptr);

  private:
    const char* _posixString;
    unsigned long _lastNtpSync;
    static const unsigned long NTP_SYNC_INTERVAL = 3600000; // 1 hour in milliseconds
};
