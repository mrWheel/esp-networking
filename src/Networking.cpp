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
*   Automatic NTP time synchronization
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
*       //-- Optional: Register WiFiManager callback for when portal starts
*       networking->doAtWiFiPortalStart([]() 
*       {
*           digitalWrite(LED_BUILTIN, LOW);  // Turn on LED
*           Serial.println("WiFi configuration portal started");
*       });
*    
*       //-- Parameters: hostname, resetWiFi pin, serial object, baud rate
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
*   Hold 'resetWiFi' pin LOW during boot to clear WiFi settings
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
*   Automatic NTP time synchronization
*
* NTP Time Management:
*   ntpStart(): Initializes NTP with timezone and optional custom servers
*   ntpIsValid(): Checks if valid time is available
*   ntpGetEpoch(): Get current epoch time
*   ntpGetData(): Get current date (YYYY-MM-DD)
*   ntpGetTime(): Get current time (HH:MM:SS)
*   ntpGetDateTime(): Get current date and time (YYYY-MM-DD HH:MM:SS)
*   ntpGetTmStruct(): Get time as tm struct
*
* Time synchronization:
*   - Initial sync done in ntpStart()
*   - Automatic hourly sync in background
*   - Timezone support using POSIX timezone strings
*   - Temporary timezone changes without affecting default
*
********************************************************************************/

#include "Networking.h"

Networking::Networking() 
    : _hostname(nullptr), _resetWiFiPin(-1), _serial(nullptr), 
      _telnetServer(nullptr), _multiStream(nullptr), 
      _onStartOTA(nullptr), _onProgressOTA(nullptr), _onEndOTA(nullptr), 
      _onWiFiPortalStart(nullptr), _posixString(nullptr), _lastNtpSync(0)
#ifdef USE_ASYNC_WIFIMANAGER
    , _webServer(nullptr), _dnsServer(nullptr)
#endif
{}

Networking::~Networking() 
{
    if (_telnetServer) delete _telnetServer;
    if (_multiStream) delete _multiStream;
#ifdef USE_ASYNC_WIFIMANAGER
    if (_webServer) delete _webServer;
    if (_dnsServer) delete _dnsServer;
#endif
}

Stream* Networking::begin(const char* hostname, int resetWiFiPin, HardwareSerial& serial, long serialSpeed, std::function<void()> wifiCallback) 
{
    _hostname = hostname;
    _resetWiFiPin = resetWiFiPin;
    _serial = &serial;

    serial.begin(serialSpeed);

#ifdef USE_ASYNC_WIFIMANAGER
    _webServer = new AsyncWebServer(80);
    _dnsServer = new DNSServer();
    AsyncWiFiManager wifiManager(_webServer, _dnsServer);
#else
    WiFiManager wifiManager;
#endif

    if (resetWiFiPin >= 0)
    {
        pinMode(resetWiFiPin, INPUT_PULLUP);
        if (digitalRead(resetWiFiPin) == LOW)
        {
            wifiManager.resetSettings();
        }
    }

    if (!wifiManager.autoConnect(_hostname))
    {
        ESP.restart();
    }

    // Initialize Telnet Server
    _telnetServer = new WiFiServer(TELNET_PORT);
    _telnetServer->begin();

    _multiStream = new MultiStream(_serial, &_telnetClient);

    setupMDNS();
    setupOTA();

    return _multiStream; // Return stream for optional use
}

void Networking::loop() 
{
    // Handle Telnet Connections
    if (_telnetServer) 
    {
        if (!_telnetClient || !_telnetClient.connected()) 
        {
            _telnetClient = _telnetServer->available();
        }
    }

#ifdef USE_ASYNC_WIFIMANAGER
    _dnsServer->processNextRequest();
#endif

    // Automatically send status updates via MultiStream
    static unsigned long lastLogTime = 0;
    if (millis() - lastLogTime > 1000) // Log every second
    {
        lastLogTime = millis();
        
        char buffer[128];
        int len = snprintf(buffer, sizeof(buffer), "Uptime: %lu ms\n", millis());

        if (len > 0)
        {
            _multiStream->write(reinterpret_cast<const uint8_t*>(buffer), len);
        }
    }

    // OTA Handling
    ArduinoOTA.handle();

    // Handle NTP Sync
    if (_posixString && millis() - _lastNtpSync > NTP_SYNC_INTERVAL)
    {
        _lastNtpSync = millis();
        ntpStart(_posixString);
    }
}

IPAddress Networking::getIPAddress() const 
{
    return WiFi.localIP();
}

String Networking::getIPAddressString() const 
{
    return WiFi.localIP().toString();
}

bool Networking::isConnected() const 
{
    return WiFi.status() == WL_CONNECTED;
}

void Networking::setupMDNS() 
{
    if (MDNS.begin(_hostname)) 
    {
        MDNS.addService("http", "tcp", 80);
    }
}

void Networking::setupOTA() 
{
    ArduinoOTA.setHostname(_hostname);
    ArduinoOTA.setPort(OTA_PORT);

    ArduinoOTA.onStart([this]() {
        if (_onStartOTA) _onStartOTA();
    });

    ArduinoOTA.onEnd([this]() {
        if (_onEndOTA) _onEndOTA();
    });

    ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
        if (_onProgressOTA) _onProgressOTA();
    });

    ArduinoOTA.begin();
}

void Networking::doAtStartOTA(std::function<void()> callback) { _onStartOTA = callback; }
void Networking::doAtProgressOTA(std::function<void()> callback) { _onProgressOTA = callback; }
void Networking::doAtEndOTA(std::function<void()> callback) { _onEndOTA = callback; }
void Networking::doAtWiFiPortalStart(std::function<void()> callback) { _onWiFiPortalStart = callback; }

bool Networking::ntpStart(const char* posixString, const char** ntpServers) 
{
    _posixString = posixString;

#ifdef ESP8266
    configTime(_posixString, 
               ntpServers ? ntpServers[0] : "pool.ntp.org",
               ntpServers ? ntpServers[1] : "time.nist.gov",
               ntpServers ? ntpServers[2] : "time.google.com");
#else
    // Extract GMT offset from POSIX string (assumes format "UTC±hh:mm")
    long gmtOffset_sec = 0;
    int daylightOffset_sec = 0;

    if (posixString && sscanf(posixString, "UTC%ld", &gmtOffset_sec) == 1) 
    {
        gmtOffset_sec *= 3600; // Convert hours to seconds
    }

    configTime(gmtOffset_sec, daylightOffset_sec, 
               ntpServers ? ntpServers[0] : "pool.ntp.org",
               ntpServers ? ntpServers[1] : "time.nist.gov",
               ntpServers ? ntpServers[2] : "time.google.com");
#endif

    struct tm timeinfo;
    return getLocalTime(&timeinfo);
}

bool Networking::ntpIsValid() const 
{
    struct tm timeinfo;
    return getLocalTime(&timeinfo);
}

time_t Networking::ntpGetEpoch(const char* posixString) 
{
    if (posixString) ntpStart(posixString);
    return time(nullptr);
}

const char* Networking::ntpGetData(const char* posixString) 
{
    static char buffer[32];
    time_t now = ntpGetEpoch(posixString);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d", localtime(&now));
    return buffer;
}

const char* Networking::ntpGetTime(const char* posixString) 
{
    static char buffer[16];
    time_t now = ntpGetEpoch(posixString);
    strftime(buffer, sizeof(buffer), "%H:%M:%S", localtime(&now));
    return buffer;
}

const char* Networking::ntpGetDateTime(const char* posixString) 
{
    static char buffer[32];
    time_t now = ntpGetEpoch(posixString);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localtime(&now));
    return buffer;
}

struct tm Networking::ntpGetTmStruct(const char* posixString) 
{
    time_t now = ntpGetEpoch(posixString);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    return timeinfo;
}