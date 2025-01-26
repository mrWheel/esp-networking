/********************************************************************************
* The Networking class is a network management solution for ESP8266/ESP32 
* microcontrollers. 
* 
* Here's how it works and how to use it:
*
* Key Features:
*   WiFi Management using WiFiManager for easy configuration
*   OTA (Over-The-Air) updates support
*   MDNS for local network discovery
*   Built-in telnet server for remote debugging
*   Combined serial and telnet output streaming
*
* Basic Setup:
*   #include "Networking.h"
#include <time.h>

#ifdef ESP8266
    #include <TZ.h>
    #include <coredecls.h>
#endif
*
*   Networking* networking = nullptr;
*   Stream* debug = nullptr;
*
*   void setup() 
*   {
*       networking = new Networking();
*    
*       //-- Optional: Register WiFiManager callback for when portal starts
*       networking->doAtWiFiPortalStart([]() 
*       {
*           digitalWrite(LED_BUILTIN, LOW);  // Turn on LED
*           Serial.println("WiFi configuration portal started");
*       });
*    
*       //-- Parameters: hostname, reset pin, serial object, baud rate
*       debug = networking->begin("esp8266", 0, Serial, 115200);
*    
*       if (!debug) 
*       {
*           //-- Restart if connection fails
*           ESP.restart(); // Restart if connection fails
*       }
*   }
*
*   void loop() 
*   {
*       //-- Must be called in main loop
*       networking->loop(); 
*     
*       //-- Use debug stream for output
*       debug->println("Hello World");
*   }
*
* WiFi Configuration:
*   First boot: Creates an access point named by hostname
*   Connect to this AP to configure WiFi credentials
*   Hold reset pin LOW during boot to clear WiFi settings
*
* Remote Debug:
*   Telnet server runs on port 23
*   Connect using any telnet client: telnet device-ip-address
*   All debug output goes to both Serial and Telnet
*   OTA Updates:
*
* Network Status:
*
*   if (networking->isConnected()) 
*   {
*       debug->print("IP Address: ");
*       debug->println(networking->getIPAddressString());
*   }
*
* Important Methods:
*   begin(): Initializes networking with hostname, reset pin, serial settings and portalCallBack function
*   loop(): Handles network events, must be called in main loop
*   isConnected(): Checks WiFi connection status
*   getIPAddress(): Returns IP as IPAddress object
*   getIPAddressString(): Returns IP as String
*
* The class automatically handles:
*   WiFi connection management
*   Telnet server for remote debugging
*   MDNS for network discovery
*   OTA update capability
*   Combined serial/telnet output streaming
*
********************************************************************************/

#include "Networking.h"

//-- Networking implementation
Networking::Networking() 
    : _hostname(nullptr), _resetPin(-1), _serial(nullptr),
      _telnetServer(nullptr), _multiStream(nullptr),
      _onStartOTA(nullptr), _onProgressOTA(nullptr), _onEndOTA(nullptr),
      _onWiFiPortalStart(nullptr), _posixString(nullptr) {}

Networking::~Networking() 
{
    if (_telnetServer) 
    {
        delete _telnetServer;
    }
    if (_multiStream) 
    {
        delete _multiStream;
    }
}

void Networking::setupMDNS() 
{
    if (MDNS.begin(_hostname)) 
    {
        _multiStream->println("MDNS responder started");
        MDNS.addService("telnet", "tcp", TELNET_PORT);
        //-- Add Arduino OTA service
        MDNS.addService("arduino", "tcp", OTA_PORT);
    } 
    else 
    {
        _multiStream->println("Error setting up MDNS responder!");
    }
}

void Networking::doAtStartOTA(std::function<void()> callback)
{
    _onStartOTA = callback;
}

void Networking::doAtProgressOTA(std::function<void()> callback)
{
    _onProgressOTA = callback;
}

void Networking::doAtEndOTA(std::function<void()> callback)
{
    _onEndOTA = callback;
}

void Networking::doAtWiFiPortalStart(std::function<void()> callback)
{
    _onWiFiPortalStart = callback;
}

void Networking::setupOTA() 
{
    //-- Configure ArduinoOTA
    ArduinoOTA.setHostname(_hostname);
    
    ArduinoOTA.onStart([this]() 
    {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "firmware" : "filesystem";
        _multiStream->printf("Start updating %s\n", type.c_str());
        if (_onStartOTA)
        {
            _onStartOTA();
        }
    });
    
    ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) 
    {
        _multiStream->printf("Progress: %u%%\r", (progress / (total / 100)));
        if (_onProgressOTA && (progress % (total / 10) < (total / 100)))
        {
            _onProgressOTA();
        }
    });
    
    ArduinoOTA.onEnd([this]() 
    {
        _multiStream->println("\nUpdate complete!");
        if (_onEndOTA)
        {
            _onEndOTA();
        }
    });
    
    ArduinoOTA.onError([this](ota_error_t error) 
    {
        _multiStream->printf("Error[%u]: ", error);
        switch (error) {
            case OTA_AUTH_ERROR:
                _multiStream->println("Auth Failed");
                break;
            case OTA_BEGIN_ERROR:
                _multiStream->println("Begin Failed");
                break;
            case OTA_CONNECT_ERROR:
                _multiStream->println("Connect Failed");
                break;
            case OTA_RECEIVE_ERROR:
                _multiStream->println("Receive Failed");
                break;
            case OTA_END_ERROR:
                _multiStream->println("End Failed");
                break;
        }
    });
    
    ArduinoOTA.begin();
    _multiStream->println("OTA ready");
}

Stream* Networking::begin(const char* hostname, int resetPin
                            , HardwareSerial& serial, long serialSpeed
                            , std::function<void()> wifiCallback) 
{
    _hostname = hostname;
    _resetPin = resetPin;
    _serial = &serial;
    
    //-- Initialize Serial
    serial.begin(serialSpeed);
    delay(100);

    //-- Initialize telnet server
    _telnetServer = new WiFiServer(TELNET_PORT);
    
    //-- Initialize MultiStream
    _multiStream = new MultiStream(&serial, &_telnetClient);
    
    //-- Initialize reset pin
    pinMode(_resetPin, INPUT_PULLUP);

    //-- Check if reset is requested
    if (digitalRead(_resetPin) == LOW) 
    {
        _multiStream->println("Reset button pressed, clearing WiFi settings...");
        WiFiManager wifiManager;
        wifiManager.resetSettings();
        _multiStream->println("Settings cleared!");
    }

    //-- Initialize WiFiManager
    WiFiManager wifiManager;
    #ifdef ESP8266
        wifiManager.setHostname(_hostname);
    #else
        WiFi.setHostname(_hostname);
    #endif

    if (wifiCallback)
    {
        _onWiFiPortalStart = wifiCallback;
        wifiManager.setAPCallback([this](WiFiManager* mgr) 
        {
            //-- WiFiManager callback function is given
            _onWiFiPortalStart();
        });
    }

    //-- Try to connect to WiFi or start config portal
    if (!wifiManager.autoConnect(_hostname)) 
    {
        _multiStream->println("Failed to connect and hit timeout");
        delay(3000);
        return nullptr;
    }

    _multiStream->println("Connected to WiFi!");
    _multiStream->print("IP address: ");
    _multiStream->println(getIPAddressString());

    //-- Setup MDNS
    setupMDNS();

    //-- Setup OTA
    setupOTA();

    //-- Start telnet server
    _telnetServer->begin();
    _telnetServer->setNoDelay(true);
    _multiStream->println("Telnet server started");

    return _multiStream;
}

void Networking::loop() 
{
    //-- Handle OTA
    ArduinoOTA.handle();
    
    //-- Handle MDNS
    #ifdef ESP8266
        MDNS.update();
    #endif

    //-- Handle telnet connections
    if (_telnetServer->hasClient()) 
    {
        //-- If a client is already connected, disconnect it
        if (_telnetClient && _telnetClient.connected()) 
        {
            _telnetClient.stop();
        }
        
        _telnetClient = _telnetServer->available();
        _telnetClient.printf("Welcome to [%s] Telnet Server!\r\n", _hostname);
    }

    //-- Handle disconnections
    if (_telnetClient && !_telnetClient.connected()) 
    {
        _telnetClient.stop();
    }
}

/**
 * Get the local IP address of the device.
 *
 * @return The local IP address as an IPAddress object.
 */
IPAddress Networking::getIPAddress() const 
{
    return WiFi.localIP();
}

/**
 * Get the local IP address of the device as a string.
 *
 * @return The local IP address as a String object.
 */
String Networking::getIPAddressString() const 
{
    return WiFi.localIP().toString();
}

/**
 * Check if the device is connected to a WiFi network.
 *
 * @return True if the device is connected to a WiFi network, false otherwise.
 */
bool Networking::isConnected() const 
{
    return WiFi.status() == WL_CONNECTED;
}

bool Networking::ntpStart(const char* posixString, const char** ntpServers)
{
    if (!isConnected())
    {
        return false;
    }

    _posixString = posixString;
    
    #ifdef ESP8266
        if (ntpServers == nullptr)
        {
            configTime(0, 0, "pool.ntp.org", "time.nist.gov");
        }
        else
        {
            configTime(0, 0, "pool.ntp.org", ntpServers[0]);
        }
    #else
        // ESP32 uses configTzTime
        configTzTime(posixString, "pool.ntp.org", ntpServers ? ntpServers[0] : "time.nist.gov");
    #endif
    
    setenv("TZ", posixString, 1);
    tzset();

    return true;
}

time_t Networking::ntpGetEpoch(const char* posixString)
{
    if (posixString)
    {
        setenv("TZ", posixString, 1);
        tzset();
    }
    else if (!_posixString)
    {
        return 0;
    }

    return time(nullptr);
}

const char* Networking::ntpGetData(const char* posixString)
{
    static char buffer[32];
    time_t now = ntpGetEpoch(posixString);
    if (now == 0)
    {
        return nullptr;
    }
    
    strftime(buffer, sizeof(buffer), "%Y-%m-%d", localtime(&now));
    return buffer;
}

const char* Networking::ntpGetTime(const char* posixString)
{
    static char buffer[32];
    time_t now = ntpGetEpoch(posixString);
    if (now == 0)
    {
        return nullptr;
    }
    
    strftime(buffer, sizeof(buffer), "%H:%M:%S", localtime(&now));
    return buffer;
}

const char* Networking::ntpGetDateTime(const char* posixString)
{
    static char buffer[32];
    time_t now = ntpGetEpoch(posixString);
    if (now == 0)
    {
        return nullptr;
    }
    
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localtime(&now));
    return buffer;
}

struct tm Networking::ntpGetTmStruct(const char* posixString)
{
    time_t now = ntpGetEpoch(posixString);
    if (now == 0)
    {
        struct tm empty = {};
        return empty;
    }
    
    return *localtime(&now);
}
