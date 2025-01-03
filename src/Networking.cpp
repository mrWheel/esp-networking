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
*  Combined serial and telnet output streaming
*
* Basic Setup:
*   #include "Networking.h"
*
*   Networking* networking = nullptr;
*   Stream* debug = nullptr;
*
*   void setup() 
*   {
*       networking = new Networking();
*    
*       //-- Parameters: hostname, reset pin, serial object, baud rate
*       debug = networking->begin("esp8266", 0, Serial, 115200);
*    
*       if (!debug) 
*       {
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
*   begin(): Initializes networking with hostname and serial settings
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
*  Combined serial/telnet output streaming
*
********************************************************************************/

#include "Networking.h"

//-- Networking implementation
Networking::Networking() 
    : _hostname(nullptr), _resetPin(-1), _serial(nullptr),
      _telnetServer(nullptr), _multiStream(nullptr) {}

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

void Networking::setupOTA() 
{
    //-- Configure ArduinoOTA
    ArduinoOTA.setHostname(_hostname);
    
    ArduinoOTA.onStart([this]() 
    {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "firmware" : "filesystem";
        _multiStream->printf("Start updating %s\n", type.c_str());
    });
    
    ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) 
    {
        _multiStream->printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    
    ArduinoOTA.onEnd([this]() 
    {
        _multiStream->println("\nUpdate complete!");
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
                            , HardwareSerial& serial, long serialSpeed) 
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
        #ifdef ESP8266
        _telnetClient.println("Welcome to ESP8266 Telnet Server!");
        #else
        _telnetClient.println("Welcome to ESP32 Telnet Server!");
        #endif
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
