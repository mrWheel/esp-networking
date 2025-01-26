# ESP8266/ESP32 Networking Library

A networking solution for ESP8266/ESP32 microcontrollers that simplifies WiFi management, remote debugging, and OTA updates.

## Features

- 🌐 **WiFi Management**: Easy WiFi configuration using WiFiManager
- 🔄 **OTA Updates**: Over-The-Air firmware updates
- 🔍 **MDNS Support**: Local network device discovery
- 📡 **Remote Debugging**: Built-in telnet server
- 📤 **Dual Output**: Combined serial and telnet output streaming
- 🔌 **Cross-Platform**: Compatible with both ESP8266 and ESP32
- ⏰ **Time Management**: Automatic NTP synchronization with timezone support

## Installation

1. Install this library in your Arduino/PlatformIO project
2. For the Arduino IDE include the necessary dependencies:
   - WiFiManager
   - ESP8266WiFi (for ESP8266) or WiFi (for ESP32)
   - ESP8266mDNS (for ESP8266) or ESPmDNS (for ESP32)
   - ArduinoOTA

## Basic Usage

```cpp
#include "Networking.h"

Networking* networking = nullptr;
Stream* debug = nullptr;

void setup()
{
    networking = new Networking();
    
    //-- Parameters: hostname, resetWiFi pin, serial object, baud rate
    debug = networking->begin("esp8266", 0, Serial, 115200);
    
    if (!debug)
    {
      //-- Restart if connection fails
      ESP.restart(); 
    }
}

void loop()
{
    //-- Must be called in main loop
    networking->loop();
    
    //-- Use debug stream for output
    debug->println("Hello World");
}
```

## WiFi Configuration

1. **First Boot**:
   - Device creates an access point named by the hostname
   - Connect to this AP with your phone/computer
   - Configure your WiFi credentials through the captive portal

2. **Reset WiFi Settings**:
   - Hold the `resetWiFiPin` LOW during boot
   - WiFi settings will be cleared
   - Device will return to AP mode for reconfiguration

## Remote Debugging

1a. **Connect via Telnet**:
   ```bash
   telnet device-ip-address
   ```
   Default port: 23
   
1b. **Connect via nc**:
   ```bash
   nc device-ip-address 23
   ```
or instead of `device-ip-address` use `mDNSname`.local

3. **Features**:
   - All debug output is mirrored to both Serial and Telnet
   - Multiple debugging sessions supported
   - Automatic session management

## Extended OTA and WiFiManager Usage

The library provides callback functions for OTA (Over-The-Air) update events and WiFiManager portal activation, allowing you to execute custom code at specific points:

```cpp
void setup()
{
    networking = new Networking();
    debug = networking->begin("esp8266", 0, Serial, 115200);
    
    //-- Register OTA callbacks
    networking->doAtStartOTA([]() 
    {
        //-- Called when OTA update starts
        digitalWrite(LED_BUILTIN, LOW);  // Turn on LED
        debug->println("OTA update starting...");
    });
    
    networking->doAtProgressOTA([]() 
    {
        //-- Called at every 20% progress
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));  // Toggle LED
        debug->println("OTA: Another 20% completed");
    });
    
    networking->doAtEndOTA([]() 
    {
        //-- Called when OTA update is about to end
        digitalWrite(LED_BUILTIN, HIGH);  // Turn off LED
        debug->println("OTA update finishing...");
    });

    //-- Register WiFiManager callback
    networking->doAtWiFiPortalStart([]() 
    {
        //-- Called when WiFiManager portal is started (no valid SSID found)
        digitalWrite(LED_BUILTIN, LOW);  // Turn on LED
        debug->println("WiFi configuration portal started");
    });
}
```

These callbacks enable you to:
- Prepare your device when an update starts (e.g., save critical data, stop ongoing operations)
- Monitor progress at 20% intervals (e.g., update a display, toggle indicators)
- Perform cleanup operations before the update completes
- Provide visual feedback during updates using LEDs or displays
- Log update progress to external systems or storage

## Network Status

```cpp
if (networking->isConnected())
{
    debug->print("IP Address: ");
    debug->println(networking->getIPAddressString());
}
```

## NTP Time Management

The library provides comprehensive NTP (Network Time Protocol) functionality with automatic synchronization:

```cpp
void setup()
{
    networking = new Networking();
    debug = networking->begin("esp32", 0, Serial, 115200);
    
    //-- Start NTP with Amsterdam timezone
    if (networking->ntpStart("CET-1CEST,M3.5.0,M10.5.0/3"))
    {
        debug->println("NTP started successfully");
    }

    //-- Optional: Use custom NTP servers
    const char* ntpServers[] = {"time.google.com", "time.cloudflare.com", nullptr};
    networking->ntpStart("CET-1CEST,M3.5.0,M10.5.0/3", ntpServers);
}

void loop()
{
    networking->loop();  // Handles automatic hourly NTP sync

    if (networking->ntpIsValid())
    {
        debug->print("Current time: ");
        debug->println(networking->ntpGetDateTime());
    }
}
```

### Features:
- Automatic hourly synchronization
- Timezone support using POSIX timezone strings
- Support for custom NTP servers
- Temporary timezone changes without affecting default
- Various time format outputs (epoch, date, time, datetime)

### Time Zones
Time zones are specified using POSIX timezone strings. Common examples:
- Central European Time: "CET-1CEST,M3.5.0,M10.5.0/3"
- Eastern Time: "EST5EDT,M3.2.0,M11.1.0"
- UTC: "UTC0"
- Japan: "JST-9"

## API Reference

### Constructor
```cpp
Networking();
```

### Main Methods

#### begin()
```cpp
Stream* begin(const char* hostname, int resetPin, HardwareSerial& serial, long serialSpeed);
```
or
```cpp
Stream* begin(const char* hostname, int resetPin, HardwareSerial& serial, long serialSpeed,
                                                       std::function<void()> wifiPortalStart);
```
Initializes the networking features:
- `hostname`: Device name for network identification
- `resetPin`: GPIO pin for WiFi reset functionality
- `serial`: Serial interface for debugging
- `serialSpeed`: Baud rate for serial communication
- `wifiPortalStart`: Sets callback function if WiFi Portal starts
- Returns: Stream object for debug output

#### loop()
```cpp
void loop();
```
Must be called in the main loop to handle:
- OTA updates
- MDNS updates
- Telnet connections
- Network events
- NTP synchronization

### Status Methods

#### isConnected()
```cpp
bool isConnected() const;
```
Returns true if connected to WiFi

#### getIPAddress()
```cpp
IPAddress getIPAddress() const;
```
Returns the current IP address as an IPAddress object

#### getIPAddressString()
```cpp
String getIPAddressString() const;
```
Returns the current IP address as a string

### NTP Methods

#### ntpStart()
```cpp
bool ntpStart(const char* posixString, const char** ntpServers = nullptr);
```
Initializes NTP with timezone and optional custom servers:
- `posixString`: POSIX timezone string
- `ntpServers`: Optional array of custom NTP server addresses
- Returns: true if time sync successful

#### ntpIsValid()
```cpp
bool ntpIsValid() const;
```
Returns true if valid time is available from NTP

#### ntpGetEpoch()
```cpp
time_t ntpGetEpoch(const char* posixString = nullptr);
```
Returns current epoch time, optionally in a different timezone

#### ntpGetData()
```cpp
const char* ntpGetData(const char* posixString = nullptr);
```
Returns current date (YYYY-MM-DD)

#### ntpGetTime()
```cpp
const char* ntpGetTime(const char* posixString = nullptr);
```
Returns current time (HH:MM:SS)

#### ntpGetDateTime()
```cpp
const char* ntpGetDateTime(const char* posixString = nullptr);
```
Returns current date and time (YYYY-MM-DD HH:MM:SS)

#### ntpGetTmStruct()
```cpp
struct tm ntpGetTmStruct(const char* posixString = nullptr);
```
Returns time as tm struct for custom formatting

### OTA and WiFiManager Callback Methods

#### doAtStartOTA()
```cpp
void doAtStartOTA(std::function<void()> callback);
```
Registers a callback function that is called when an OTA update starts

#### doAtProgressOTA()
```cpp
void doAtProgressOTA(std::function<void()> callback);
```
Registers a callback function that is called at every 20% progress during OTA update

#### doAtEndOTA()
```cpp
void doAtEndOTA(std::function<void()> callback);
```
Registers a callback function that is called when an OTA update is about to end

#### doAtWiFiPortalStart()
```cpp
void doAtWiFiPortalStart(std::function<void()> callback);
```
Registers a callback function that is called when WiFiManager portal is started (no valid SSID found)

## Automatic Features

The library handles these features automatically:
- WiFi connection management
- Telnet server for remote debugging
- MDNS for network discovery
- OTA update capability
- Combined serial/telnet output streaming
- Hourly NTP time synchronization

## License

This project is licensed under the MIT License - see the LICENSE file for details.
