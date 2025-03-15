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


# ESP Networking Library API Documentation

## What is an API?

An API (Application Programming Interface) is a set of clearly defined methods, functions, and protocols that developers can use to communicate with specific software or hardware. An API acts as a bridge between different software components, allowing them to communicate with each other without the developer needing to understand the internal workings of each component.

In the context of this library, the API provides a simple way to implement network functionality in ESP8266/ESP32 projects, without having to worry about the complex details of WiFi connections, OTA updates, telnet servers, or NTP time synchronization.

## Networking Library Overview

The ESP Networking library provides a comprehensive set of functions for managing network connections on ESP8266 and ESP32 microcontrollers. The library simplifies the process of:

- WiFi connection and configuration
- Over-the-Air (OTA) firmware updates
- Telnet server for remote debugging
- mDNS service discovery
- NTP time synchronization
- Automatic WiFi reconnection

## Main Classes

### Networking Class

The `Networking` class is the primary interface for all network functionality.

#### Initialization

```cpp
Networking* networking = new Networking();
Stream* debug = networking->begin("my-esp", 0, Serial, 115200);

// Optional: callback for WiFi portal
networking->doAtWiFiPortalStart([]() 
{
  Serial.println("WiFi configuration portal started");
});
```

#### Important Methods

##### `begin(hostname, resetWiFiPin, serial, serialSpeed, wifiCallback)`

Initializes the networking functionality.

- `hostname`: The name by which the device is visible on the network
- `resetWiFiPin`: GPIO pin used to reset WiFi settings (connect to GND to reset)
- `serial`: HardwareSerial object for debug output
- `serialSpeed`: Baud rate for serial communication
- `wifiCallback`: Optional callback function that is called when the WiFi configuration portal starts

```cpp
// Example: Initialization with callback for WiFi portal
Stream* debug = networking->begin("esp32-project", 0, Serial, 115200, []() 
{
  Serial.println("WiFi configuration portal started!");
  // Here you could, for example, make an LED blink
});
```

Or you can do

```cpp
void callBackWiFiPortal()
{
  Serial.println("WiFi configuration portal started");
  digitalWrite(LED_BUILTIN, HIGH);
}

// Example: Initialization with callback for WiFi portal
Stream* debug = networking->begin("esp32-project", 0, Serial, 115200, callBackWiFiPortal);
```


##### `loop()`

Must be called regularly in the main loop of your program to handle network events.

```cpp
void loop() {
  networking->loop(); // Required for proper operation
  
  // Your own code here
}
```

##### WiFi-related methods

```cpp
// Check if WiFi is connected
if (networking->isConnected()) {
  Serial.println("Connected to WiFi");
}

// Get the IP address as an IPAddress object
IPAddress ip = networking->getIPAddress();

// Get the IP address as a string
String ipStr = networking->getIPAddressString();
Serial.print("IP address: ");
Serial.println(ipStr); // For example: "192.168.1.105"

// Manually reconnect to WiFi (if needed)
networking->reconnectWiFi();
```

##### OTA Update Callbacks

```cpp
// Register callbacks for OTA updates
networking->doAtStartOTA([]() {
  Serial.println("OTA update started");
  // For example: turn on LED to indicate update in progress
});

networking->doAtProgressOTA([]() {
  Serial.println("OTA update progress");
  // Called approximately every 20%
});

networking->doAtEndOTA([]() {
  Serial.println("OTA update completed");
  // For example: turn off LED
});
```

##### NTP Time Synchronization

```cpp
// Start NTP with timezone for Amsterdam
if (networking->ntpStart("CET-1CEST,M3.5.0,M10.5.0/3")) {
  Serial.println("NTP successfully started");
}

// Optional: specify custom NTP servers
const char* ntpServers[] = {"time.google.com", "time.cloudflare.com", nullptr};
networking->ntpStart("CET-1CEST,M3.5.0,M10.5.0/3", ntpServers);

// Check if NTP time is valid
if (networking->ntpIsValid()) {
  Serial.println("NTP time is valid");
}

// Get current date (YYYY-MM-DD)
Serial.print("Date: ");
Serial.println(networking->ntpGetDate()); // For example: "2025-03-15"

// Get current time (HH:MM:SS)
Serial.print("Time: ");
Serial.println(networking->ntpGetTime()); // For example: "14:30:25"

// Get current date and time (YYYY-MM-DD HH:MM:SS)
Serial.print("Date and time: ");
Serial.println(networking->ntpGetDateTime()); // For example: "2025-03-15 14:30:25"

// Get epoch time (seconds since 1-1-1970)
time_t epoch = networking->ntpGetEpoch();
Serial.print("Epoch: ");
Serial.println(epoch); // For example: "1742265025"

// Get time in another timezone (for example New York)
Serial.print("New York time: ");
Serial.println(networking->ntpGetDateTime("EST5EDT,M3.2.0,M11.1.0"));

// Get detailed time information as tm struct
struct tm timeInfo = networking->ntpGetTmStruct();
Serial.printf("Year: %d\n", timeInfo.tm_year + 1900);
Serial.printf("Month: %d\n", timeInfo.tm_mon + 1);
Serial.printf("Day: %d\n", timeInfo.tm_mday);
Serial.printf("Hour: %d\n", timeInfo.tm_hour);
Serial.printf("Minute: %d\n", timeInfo.tm_min);
Serial.printf("Second: %d\n", timeInfo.tm_sec);
```

### MultiStream Class

The `MultiStream` class is used internally to direct output to both the serial port and telnet clients. You typically don't need to use this class directly, but you can use the `Stream*` object returned by `begin()` for all debug output.

```cpp
// Use the debug object for all output
debug->println("This text appears on both the serial monitor and via telnet");
debug->printf("Formatted output: %d, %s\n", 42, "text");
```

## Complete Examples

### Basic Example

```cpp
#include "Networking.h"

Networking* networking = nullptr;
Stream* debug = nullptr;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Initialize networking
  networking = new Networking();
  debug = networking->begin("my-esp", 0, Serial, 115200);
  
  if (!debug) {
    ESP.restart(); // Restart if connection fails
  }
  
  debug->println("Network initialized!");
  debug->print("IP address: ");
  debug->println(networking->getIPAddressString());
}

void loop() {
  // Required: process network events
  networking->loop();
  
  // Your code here
  delay(1000);
  debug->println("Hello world!");
}
```

### NTP Example

```cpp
#include "Networking.h"

Networking* networking = nullptr;
Stream* debug = nullptr;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Initialize networking
  networking = new Networking();
  debug = networking->begin("my-esp", 0, Serial, 115200);
  
  if (!debug) {
    ESP.restart();
  }
  
  // Start NTP with Amsterdam timezone
  if (networking->ntpStart("CET-1CEST,M3.5.0,M10.5.0/3")) {
    debug->println("NTP started");
  }
}

void loop() {
  networking->loop();
  
  // Show time every 5 seconds
  static unsigned long lastTime = 0;
  if (millis() - lastTime > 5000) {
    lastTime = millis();
    
    if (networking->ntpIsValid()) {
      debug->print("Current time: ");
      debug->println(networking->ntpGetDateTime());
    } else {
      debug->println("Waiting for valid time...");
    }
  }
}
```

### OTA Update Example

```cpp
#include "Networking.h"

Networking* networking = nullptr;
Stream* debug = nullptr;
bool updateInProgress = false;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Initialize networking
  networking = new Networking();
  
  // Register OTA callbacks
  networking->doAtStartOTA([]() {
    updateInProgress = true;
    digitalWrite(LED_BUILTIN, HIGH); // LED on during update
  });
  
  networking->doAtEndOTA([]() {
    updateInProgress = false;
    digitalWrite(LED_BUILTIN, LOW); // LED off after update
  });
  
  // Start networking
  debug = networking->begin("my-esp", 0, Serial, 115200);
  
  if (!debug) {
    ESP.restart();
  }
  
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  
  debug->println("Ready for OTA updates!");
  debug->print("IP address: ");
  debug->println(networking->getIPAddressString());
}

void loop() {
  networking->loop();
  
  // Normal operation when no update is in progress
  if (!updateInProgress) {
    // Your code here
    delay(1000);
    debug->println("System running normally...");
  }
}
```

## Advanced Features

### WiFi Reset

The library supports resetting WiFi settings by pulling a GPIO pin low (connecting to GND). This is useful if you want to move your device to a different network.

```cpp
// During initialization, specify the reset pin (here GPIO 0)
debug = networking->begin("my-esp", 0, Serial, 115200);

// If GPIO 0 is low during startup, WiFi settings are cleared
// and the configuration portal starts
```

### Telnet Server

The library automatically starts a telnet server on port 23. You can connect to the device via telnet to see debug output and monitor without a physical connection.

```
$ telnet my-esp.local
Connected to my-esp.local.
Escape character is '^]'.
Welcome to [my-esp] Telnet Server!
```

### mDNS Service Discovery

The device is discoverable on the local network via mDNS with the hostname you specify, followed by ".local". For example: "my-esp.local".

## Timezone Formats

For NTP time synchronization, the library uses POSIX timezone strings. Here are some examples:

- Amsterdam/Netherlands: `"CET-1CEST,M3.5.0,M10.5.0/3"`
- London/UK: `"GMT0BST,M3.5.0/1,M10.5.0"`
- New York/US Eastern: `"EST5EDT,M3.2.0,M11.1.0"`
- Los Angeles/US Pacific: `"PST8PDT,M3.2.0,M11.1.0"`
- Tokyo/Japan: `"JST-9"`
- Sydney/Australia: `"AEST-10AEDT,M10.1.0,M4.1.0/3"`

## Error Handling and Reliability

The library contains built-in mechanisms for automatically reconnecting to WiFi when the connection is lost. If the maximum number of reconnection attempts (default 5) is reached, the device will automatically restart.

```cpp
// Periodically check connection status
if (!networking->isConnected()) {
  debug->println("WiFi connection lost!");
  
  // The library will automatically try to reconnect
  // You can also manually reconnect:
  networking->reconnectWiFi();
}
```

## Compatibility

The library works with both ESP8266 and ESP32 platforms and automatically detects which platform is being used. It provides a consistent API across both platforms, making your code easily portable.


## License

This project is licensed under the MIT License - see the LICENSE file for details.

