#pragma once

#ifdef ESP8266
    #include <ESP8266WiFi.h>
    #include <ESP8266mDNS.h>
#else
    #include <WiFi.h>
    #include <ESPmDNS.h>
#endif

#include <WiFiManager.h>
#include <StreamString.h>
#include <ArduinoOTA.h>
#include <functional>

class MultiStream : public Stream 
{
  private:
    Stream* _serial;
    WiFiClient* _telnetClient;

  public:
    MultiStream(Stream* serial, WiFiClient* telnetClient) 
        : _serial(serial), _telnetClient(telnetClient) {}

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
    int _resetPin;
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

  public:
    Networking();
    ~Networking();

    Stream* begin(const char* hostname, int resetPin, HardwareSerial& serial, long serialSpeed, std::function<void()> wifiCallback = nullptr);
    void loop();
    
    // IP address methods
    IPAddress getIPAddress() const;
    String getIPAddressString() const;
    bool isConnected() const;

    void doAtStartOTA(std::function<void()> callback);
    void doAtProgressOTA(std::function<void()> callback);
    void doAtEndOTA(std::function<void()> callback);
    void doAtWiFiPortalStart(std::function<void()> callback);
};
