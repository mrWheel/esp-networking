# ESP8266/ESP32 Networking Library

A networking solution for ESP8266/ESP32 microcontrollers that simplifies WiFi management, remote debugging, and OTA updates.

## Features

- 🌐 **WiFi Management**: Easy WiFi configuration using WiFiManager
- 🔄 **OTA Updates**: Over-The-Air firmware updates
- 🔍 **MDNS Support**: Local network device discovery
- 📡 **Remote Debugging**: Built-in telnet server
- 📤 **Dual Output**: Combined serial and telnet output streaming
- 🔌 **Cross-Platform**: Compatible with both ESP8266 and ESP32

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
    
    // Parameters: hostname, reset pin, serial object, baud rate
    debug = networking->begin("esp8266", 0, Serial, 115200);
    
    if (!debug)
    {
        ESP.restart(); // Restart if connection fails
    }
}

void loop()
{
    // Must be called in main loop
    networking->loop();
    
    // Use debug stream for output
    debug->println("Hello World");
}
```

## WiFi Configuration

1. **First Boot**:
   - Device creates an access point named by the hostname
   - Connect to this AP with your phone/computer
   - Configure your WiFi credentials through the captive portal

2. **Reset WiFi Settings**:
   - Hold the `resetPin` LOW during boot
   - WiFi settings will be cleared
   - Device will return to AP mode for reconfiguration

## Remote Debugging

1a. **Connect via Telnet**:
   ```bash
   telnet `device-ip-address`
   ```
   Default port: 23
   
1b. **Connect via nc**:
   ```bash
   nc `device-ip-address` 23
   ```
or instead of `device-ip-address` use `mDNSname`.local

3. **Features**:
   - All debug output is mirrored to both Serial and Telnet
   - Multiple debugging sessions supported
   - Automatic session management

## Network Status

```cpp
if (networking->isConnected())
{
    debug->print("IP Address: ");
    debug->println(networking->getIPAddressString());
}
```

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
Initializes the networking features:
- `hostname`: Device name for network identification
- `resetPin`: GPIO pin for WiFi reset functionality
- `serial`: Serial interface for debugging
- `serialSpeed`: Baud rate for serial communication
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

## Automatic Features

The library handles these features automatically:
- WiFi connection management
- Telnet server for remote debugging
- MDNS for network discovery
- OTA update capability
- Combined serial/telnet output streaming

## License

This project is licensed under the MIT License - see the LICENSE file for details.
