#include "Networking.h"

//-- MultiStream implementation
/**
 * Constructor for the MultiStream class.
 * Initializes the serial and telnet client pointers and buffer index.
 * 
 * @param serial Pointer to the serial stream
 * @param telnetClient Pointer to the telnet client
 */
MultiStream::MultiStream(Stream* serial, WiFiClient* telnetClient)
    : _serial(serial), _telnetClient(telnetClient), _bufferIndex(0), _inCriticalSection(false)
{
}

/**
 * Writes a single byte to the buffer.
 * Flushes the buffer if it's full or if a newline character is encountered.
 * 
 * @param c The byte to write
 * @return The number of bytes written
 */
size_t MultiStream::write(uint8_t c)
{
  // Add the byte to the buffer
  _buffer[_bufferIndex++] = c;
  
  // If the buffer is full or we encounter a newline, flush it
  if (_bufferIndex >= BUFFER_SIZE - 1 || c == '\n')
  {
    flushBuffer();
  }
  
  return 1;
}

/**
 * Writes a buffer of bytes directly to both streams.
 * This is more efficient than writing byte-by-byte.
 * 
 * @param buffer The buffer to write
 * @param size The number of bytes to write
 * @return The number of bytes written
 */
size_t MultiStream::write(const uint8_t* buffer, size_t size)
{
  // First, flush any pending bytes in our internal buffer
  if (_bufferIndex > 0)
  {
    flushBuffer();
  }
  
  // Write the new buffer directly to both streams
  _serial->write(buffer, size);
  
  if (_telnetClient && _telnetClient->connected())
  {
    _telnetClient->write(buffer, size);
  }
  
  // Ensure the data is sent immediately if not in a critical section
  if (!_inCriticalSection)
  {
    _serial->flush();
    if (_telnetClient && _telnetClient->connected())
    {
      _telnetClient->flush();
    }
  }
  
  return size;
}

/**
 * Flushes the internal buffer by writing its contents to both streams.
 */
void MultiStream::flushBuffer()
{
  if (_bufferIndex > 0)
  {
    // Ensure null termination for safety
    _buffer[_bufferIndex] = 0;
    
    // Write the buffer to the serial port
    _serial->write(_buffer, _bufferIndex);
    
    // Write the buffer to the telnet client if connected
    if (_telnetClient && _telnetClient->connected())
    {
      _telnetClient->write(_buffer, _bufferIndex);
    }
    
    // Ensure the data is sent immediately if not in a critical section
    if (!_inCriticalSection)
    {
      _serial->flush();
      if (_telnetClient && _telnetClient->connected())
      {
        _telnetClient->flush();
      }
    }
    
    // Reset the buffer index
    _bufferIndex = 0;
  }
}

/**
 * Flushes both streams and the internal buffer.
 */
void MultiStream::flush()
{
  // Flush our internal buffer first
  flushBuffer();
  
  // Then flush both streams
  _serial->flush();
  
  if (_telnetClient && _telnetClient->connected())
  {
    _telnetClient->flush();
  }
}

/**
 * Begin a critical section where flush is deferred until the end.
 * This is useful for high-frequency writes where you want to batch flushes.
 */
void MultiStream::beginCriticalSection()
{
  _inCriticalSection = true;
}

/**
 * End a critical section and flush any pending data.
 */
void MultiStream::endCriticalSection()
{
  _inCriticalSection = false;
  flush();
}


//-- Static instance pointer for ESP8266 callbacks
#ifdef ESP8266
Networking* Networking::_instance = nullptr;
#endif

//-- Networking implementation
/**
 * Constructor for the Networking class.
 * Initializes all member variables to their default values.
 */
Networking::Networking() 
    : _hostname(nullptr), _resetWiFiPin(-1), _serial(nullptr),
      _telnetServer(nullptr), _multiStream(nullptr),
      _onStartOTA(nullptr), _onProgressOTA(nullptr), _onEndOTA(nullptr),
      _onWiFiPortalStart(nullptr), _posixString(nullptr), _lastNtpSync(0),
      _isReconnecting(false), _reconnectAttempts(0), _lastReconnectAttempt(0)
      #ifdef USE_ASYNC_WIFIMANAGER
      , _webServer(nullptr), _dnsServer(nullptr)
      #endif
{
    #ifdef ESP8266
    _instance = this;  // Set static instance pointer for callbacks
    #endif
}

/**
 * Destructor for the Networking class.
 * Cleans up dynamically allocated resources.
 */
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
    #ifdef USE_ASYNC_WIFIMANAGER
    if (_webServer)
    {
        delete _webServer;
    }
    if (_dnsServer)
    {
        delete _dnsServer;
    }
    #endif
    
    #ifdef ESP8266
    if (_instance == this)
    {
        _instance = nullptr;
    }
    #endif
}

/**
 * Sets up Multicast DNS (mDNS) for the device.
 * Enables service discovery for telnet and OTA updates.
 */
void Networking::setupMDNS() 
{
    _multiStream->printf("Start MDNS with hostname [%s.local]\n", _hostname);
    if (MDNS.begin(_hostname)) 
    {
        // Add Telnet service
        _multiStream->printf("addService(\"telnet\", \"tcp\", %d)\n", TELNET_PORT);
        MDNS.addService("telnet", "tcp", TELNET_PORT);

        #ifdef ESP32
            // Enable the Arduino OTA service (this automatically registers the Arduino service)
            _multiStream->printf("enableArduino(%d) - this may show \"Failed adding Arduino service\"!\n", OTA_PORT);
            MDNS.enableArduino(OTA_PORT); // This should handle Arduino OTA automatically
        #else
            // Add Arduino OTA service for ESP8266
            _multiStream->printf("addService(\"arduino\", \"tcp\", %d)\n", OTA_PORT);
            MDNS.addService("arduino", "tcp", OTA_PORT);
        #endif
    } 
    else 
    {
        _multiStream->println("Error setting up MDNS responder!");
    }

} //  Networking::setupMDNS()

/**
 * Sets a callback function to be executed when OTA update starts.
 * 
 * @param callback Function to be called at the start of OTA update
 */
void Networking::doAtStartOTA(std::function<void()> callback)
{
    _onStartOTA = callback;
}

/**
 * Sets a callback function to be executed during OTA update progress.
 * 
 * @param callback Function to be called during OTA update progress
 */
void Networking::doAtProgressOTA(std::function<void()> callback)
{
    _onProgressOTA = callback;
}

/**
 * Sets a callback function to be executed when OTA update completes.
 * 
 * @param callback Function to be called at the end of OTA update
 */
void Networking::doAtEndOTA(std::function<void()> callback)
{
    _onEndOTA = callback;
}

/**
 * Sets a callback function to be executed when WiFi configuration portal starts.
 * 
 * @param callback Function to be called when WiFi portal starts
 */
void Networking::doAtWiFiPortalStart(std::function<void()> callback)
{
    _onWiFiPortalStart = callback;
}

/**
 * Configures Over-The-Air (OTA) update functionality.
 * Sets up callbacks for different OTA events and initializes the OTA system.
 */
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
        if (_onProgressOTA && (progress % (total / 5) < (total / 100)))
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

/**
 * Sets up WiFi event handlers for both ESP8266 and ESP32.
 * This improves connection reliability by responding directly to WiFi events.
 */
void Networking::setupWiFiEvents()
{
    #ifdef ESP8266
    // For ESP8266, we use the specific event handlers
    WiFi.onStationModeConnected(&Networking::_onStationModeConnected);
    WiFi.onStationModeDisconnected(&Networking::_onStationModeDisconnected);
    WiFi.onStationModeGotIP(&Networking::_onStationModeGotIP);
    
    #else
    // For ESP32, we use the generic event handler
    WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
        switch (event) {
            case SYSTEM_EVENT_STA_START:
                _multiStream->println("Networking:: WiFi station started");
                break;
                
            case SYSTEM_EVENT_STA_CONNECTED:
                _multiStream->println("Networking:: WiFi connected");
                break;
                
            case SYSTEM_EVENT_STA_DISCONNECTED:
                _multiStream->println("Networking:: WiFi disconnected");
                
                // Only attempt reconnection if we're not already trying
                if (!_isReconnecting) 
                {
                    _isReconnecting = true;
                    
                    if (_reconnectAttempts < WIFI_RECONNECT_MAX_ATTEMPTS) 
                    {
                        _multiStream->printf("Networking:: Attempting to reconnect (attempt %d of %d)...\n", 
                                            _reconnectAttempts + 1, WIFI_RECONNECT_MAX_ATTEMPTS);
                        
                        WiFi.disconnect();
                        delay(500);
                        WiFi.begin();
                        
                        _reconnectAttempts++;
                    } 
                    else 
                    {
                        _multiStream->println("Networking:: Max WiFi reconnect attempts reached! Restarting...");
                        delay(1000);
                        ESP.restart();  // Restart if unable to reconnect
                    }
                }
                break;
                
            case SYSTEM_EVENT_STA_GOT_IP:
                _multiStream->print("Networking:: WiFi got IP: ");
                _multiStream->println(WiFi.localIP().toString());
                
                // Reset reconnection variables on successful connection
                _isReconnecting = false;
                _reconnectAttempts = 0;
                break;
                
            default:
                break;
        }
    });
    #endif
}

#ifdef ESP8266
/**
 * Static event handler for ESP8266 station mode connected event.
 * 
 * @param event The WiFi event information
 */
void Networking::_onStationModeConnected(const WiFiEventStationModeConnected& event)
{
    if (_instance && _instance->_multiStream)
    {
        _instance->_multiStream->printf("Networking:: WiFi connected to SSID: %s, channel: %d\n", 
                                       event.ssid.c_str(), event.channel);
    }
}

/**
 * Static event handler for ESP8266 station mode disconnected event.
 * 
 * @param event The WiFi event information
 */
void Networking::_onStationModeDisconnected(const WiFiEventStationModeDisconnected& event)
{
    if (_instance && _instance->_multiStream)
    {
        _instance->_multiStream->printf("Networking:: WiFi disconnected from SSID: %s, reason: %d\n", 
                                       event.ssid.c_str(), event.reason);
        
        // Only attempt reconnection if we're not already trying
        if (!_instance->_isReconnecting) 
        {
            _instance->_isReconnecting = true;
            
            if (_instance->_reconnectAttempts < WIFI_RECONNECT_MAX_ATTEMPTS) 
            {
                _instance->_multiStream->printf("Networking:: Attempting to reconnect (attempt %d of %d)...\n", 
                                              _instance->_reconnectAttempts + 1, WIFI_RECONNECT_MAX_ATTEMPTS);
                
                WiFi.disconnect();
                delay(500);
                WiFi.begin();
                
                _instance->_reconnectAttempts++;
            } 
            else 
            {
                _instance->_multiStream->println("Networking:: Max WiFi reconnect attempts reached! Restarting...");
                delay(1000);
                ESP.restart();  // Restart if unable to reconnect
            }
        }
    }
}

/**
 * Static event handler for ESP8266 station mode got IP event.
 * 
 * @param event The WiFi event information
 */
void Networking::_onStationModeGotIP(const WiFiEventStationModeGotIP& event)
{
    if (_instance && _instance->_multiStream)
    {
        _instance->_multiStream->print("Networking:: WiFi got IP: ");
        _instance->_multiStream->println(event.ip.toString());
        
        // Reset reconnection variables on successful connection
        _instance->_isReconnecting = false;
        _instance->_reconnectAttempts = 0;
    }
}
#endif

/**
 * Initializes the networking functionality.
 * Sets up WiFi, MDNS, OTA updates, and telnet server.
 * 
 * @param hostname Device hostname for network identification
 * @param resetWiFiPin GPIO pin for WiFi settings reset
 * @param serial Hardware serial interface for debugging
 * @param serialSpeed Baud rate for serial communication
 * @param wifiCallback Optional callback for WiFi portal events
 * @return Pointer to Stream object for debug output, nullptr if initialization fails
 */
Stream* Networking::begin(const char* hostname, int resetWiFiPin
  , HardwareSerial& serial, long serialSpeed
  , std::function<void()> wifiCallback) 
{
    _hostname = hostname;
    _resetWiFiPin = resetWiFiPin;
    _serial = &serial;

    //-- Initialize Serial
    serial.begin(serialSpeed);
    delay(100);

    //-- Initialize telnet server
    _telnetServer = new WiFiServer(TELNET_PORT);

    //-- Initialize MultiStream
    _multiStream = new MultiStream(&serial, &_telnetClient);

    //-- Initialize reset pin
    pinMode(_resetWiFiPin, INPUT_PULLUP);

    //-- Check if reset is requested
    if (digitalRead(_resetWiFiPin) == LOW) 
    {
        _multiStream->println("Networking:: Reset button pressed, clearing WiFi settings...");
        #ifdef USE_ASYNC_WIFIMANAGER
        AsyncWiFiManager wifiManager(_webServer, _dnsServer);
        wifiManager.resetSettings();
        #else
        ::WiFiManager wifiManager;
        wifiManager.resetSettings();
        #endif
        _multiStream->println("Networking:: Settings cleared!");
    }

    //-- Setup WiFi event handlers
    setupWiFiEvents();

    //-- Try connecting to WiFi
    _multiStream->println("Networking:: Connecting to WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin();

    int retries = 20;
    while (!isConnected() && retries-- > 0) 
    {
        delay(500);
        _multiStream->print(".");
    }

    if (!isConnected()) 
    {
        _multiStream->println("\nWiFi connection failed. Starting configuration portal...");

        #ifdef USE_ASYNC_WIFIMANAGER
        _webServer = new AsyncWebServer(80);
        _dnsServer = new DNSServer();
        AsyncWiFiManager wifiManager(_webServer, _dnsServer);
        #else
        ::WiFiManager wifiManager;
        #endif

        if (wifiCallback)
        {
            _onWiFiPortalStart = wifiCallback;
            wifiManager.setAPCallback([this](::WiFiManager* mgr) 
            {
                _onWiFiPortalStart();
            });
        }

        wifiManager.setTimeout(240); // Set timeout for the portal
        if (!wifiManager.autoConnect(_hostname)) 
        {
            _multiStream->println("Networking:: Failed to connect to WiFi. Restarting...");
            delay(3000);
            ESP.restart();
            return nullptr;
        }
    }

    _multiStream->println("\nNetworking:: Connected to WiFi!");
    _multiStream->print("IP address: ");
    _multiStream->println(getIPAddressString());

    //-- Setup MDNS
    setupMDNS();

    //-- Setup OTA
    setupOTA();

    //-- Start telnet server
    _telnetServer->begin();
    _telnetServer->setNoDelay(true);
    _multiStream->println("Networking:: Telnet server started");

    return _multiStream;

} //  Networking::begin()

/**
 * Main loop function for handling network events.
 * Manages OTA updates, MDNS, telnet connections, and NTP synchronization.
 * WiFi reconnection is now handled by event handlers instead of polling.
 */
void Networking::loop() 
{
    //-- Handle OTA
    ArduinoOTA.handle();
    
    //-- Handle MDNS
    #ifdef ESP8266
        MDNS.update();
    #endif

    //-- Handle incoming telnet connections
    if (_telnetServer->hasClient()) 
    {
        WiFiClient newClient = _telnetServer->available();

        //-- If an existing client is connected, notify and disconnect it
        if (_telnetClient && _telnetClient.connected()) 
        {
            _telnetClient.println("Networking:: Telnet disconnected due to new client.");
            _telnetClient.stop();
        }

        //-- Assign the new client
        _telnetClient = newClient;
        _telnetClient.printf("Welcome to [%s] Telnet Server!\r\n", _hostname);
    }

    //-- Handle disconnections
    if (_telnetClient && !_telnetClient.connected()) 
    {
        _telnetClient.stop();
    }

    //-- Periodic NTP sync
    if (_posixString && (millis() - _lastNtpSync >= NTP_SYNC_INTERVAL))
    {
        #ifdef ESP8266
            configTime(0, 0, "pool.ntp.org", "time.nist.gov");
        #else
            configTzTime(_posixString, "pool.ntp.org", "time.nist.gov");
        #endif
        _lastNtpSync = millis();
    }

} //  Networking::loop()

/**
 * Manually triggers a WiFi reconnection.
 * This is now primarily used for manual reconnection requests,
 * as automatic reconnection is handled by event handlers.
 */
void Networking::reconnectWiFi() 
{
    if (!_isReconnecting)
    {
        _multiStream->println("Networking:: Manually reconnecting to WiFi...");
        
        _isReconnecting = true;
        WiFi.disconnect();
        delay(500);
        WiFi.begin();
        
        // Wait for connection
        int retries = 20; // Try for 10 seconds (20 x 500ms)
        while (!isConnected() && retries-- > 0) 
        {
            delay(500);
            _multiStream->print(".");
        }
        
        if (isConnected()) 
        {
            _multiStream->println("\nNetworking:: WiFi reconnected successfully!");
            _multiStream->print("Networking:: New IP: ");
            _multiStream->println(getIPAddressString());
            _isReconnecting = false;
        } 
        else 
        {
            _multiStream->println("\nNetworking:: WiFi reconnection failed.");
            // The event handlers will continue trying to reconnect
        }
    }
    else
    {
        _multiStream->println("Networking:: Reconnection already in progress...");
    }
} //  Networking::reconnectWiFi()


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

/**
 * Checks if the current NTP time is valid.
 * 
 * @return True if NTP time is valid, false otherwise
 */
bool Networking::ntpIsValid() const
{
    return time(nullptr) > 1000000;
}

/**
 * Initializes NTP time synchronization.
 * 
 * @param posixString POSIX timezone string
 * @param ntpServers Array of NTP server addresses (optional)
 * @return True if NTP initialization successful, false otherwise
 */
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

    // Wait up to 5 seconds for time sync
    int retries = 50;
    while (time(nullptr) < 1000000 && retries-- > 0)
    {
        delay(100);
    }

    if (time(nullptr) > 1000000)
    {
        _lastNtpSync = millis();
        return true;
    }
    return false;
}

/**
 * Gets current epoch time with optional timezone override.
 * 
 * @param posixString Optional POSIX timezone string for temporary timezone change
 * @return Current epoch time
 */
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
    else
    {
        setenv("TZ", _posixString, 1); // Reset to default timezone
        tzset();
    }

    return time(nullptr);
}

/**
 * Gets current date in YYYY-MM-DD format.
 * 
 * @param posixString Optional POSIX timezone string
 * @return Current date string or nullptr if time not available
 */
const char* Networking::ntpGetDate(const char* posixString)
{
    static char buffer[32];
    time_t now = ntpGetEpoch(posixString);
    if (now == 0)
    {
        return nullptr;
    }
    
    strftime(buffer, sizeof(buffer), "%Y-%m-%d", localtime(&now));
    return buffer;
} // ntpGetDate()

/**
 * Gets current date in DD-MM-YYYY format.
 * 
 * @param posixString Optional POSIX timezone string
 * @return Current date string or nullptr if time not available
 */
const char* Networking::ntpGetDateDMY(const char* posixString)
{
    static char buffer[32];
    time_t now = ntpGetEpoch(posixString);
    if (now == 0)
    {
        return nullptr;
    }
    
    strftime(buffer, sizeof(buffer), "%d-%m-%Y", localtime(&now));
    return buffer;

} //  ntpGetDateDMY()

/**
 * Gets current time in HH:MM:SS format.
 * 
 * @param posixString Optional POSIX timezone string
 * @return Current time string or nullptr if time not available
 */
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

/**
 * Gets current date and time in YYYY-MM-DD HH:MM:SS format.
 * 
 * @param posixString Optional POSIX timezone string
 * @return Current date and time string or nullptr if time not available
 */
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
/**
 * Gets current date and time in DD-MM-YYYY HH:MM:SS format.
 * 
 * @param posixString Optional POSIX timezone string
 * @return Current date and time string or nullptr if time not available
 */
const char* Networking::ntpGetDateTimeDMY(const char* posixString)
{
    static char buffer[32];
    time_t now = ntpGetEpoch(posixString);
    if (now == 0)
    {
        return nullptr;
    }
    
    strftime(buffer, sizeof(buffer), "%d-%m-%Y %H:%M:%S", localtime(&now));
    return buffer;
}

/**
 * Gets current time as a tm structure.
 * 
 * @param posixString Optional POSIX timezone string
 * @return tm structure containing current time or empty structure if time not available
 */
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
