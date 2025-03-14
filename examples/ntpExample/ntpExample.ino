#include "Networking.h"
#include <Arduino.h>

Networking* networking = nullptr;
Stream* debug = nullptr;


void showTime()
{
    //-- Get and print current date
    debug->print("\r\nCurrent Date         : ");
    debug->println(networking->ntpGetData());
    
    //-- Get and print current time
    debug->print("Current Time         : ");
    debug->println(networking->ntpGetTime());
    
    //-- Get and print current date and time
    debug->print("Current Date and Time: ");
    debug->println(networking->ntpGetDateTime());
    
    //-- Get epoch time
    time_t epoch = networking->ntpGetEpoch();
    debug->print("Epoch Time: ");
    debug->println(epoch);
    
    //-- Get tm struct and print some components
    struct tm timeInfo = networking->ntpGetTmStruct();
    debug->printf("Year  : %4d\n", timeInfo.tm_year + 1900);
    debug->printf("Month : %2d\n", timeInfo.tm_mon + 1);
    debug->printf("Day   : %2d\n", timeInfo.tm_mday);
    debug->printf("Hour  : %2d\n", timeInfo.tm_hour);
    debug->printf("Minute: %2d\n", timeInfo.tm_min);
    debug->printf("Second: %2d\n", timeInfo.tm_sec);
    
    //-- Example with different timezone (New York)
    debug->print("\nNew York Time        : ");
    debug->println(networking->ntpGetDateTime("EST5EDT,M3.2.0,M11.1.0"));

}


void setup() 
{
    Serial.begin(115200);
    while(!Serial) { delay(10); }
    delay(5000);
    Serial.println("\nStarting ntpExample ...\n");

    networking = new Networking();
    
    //-- Optional: Register WiFiManager callback for when portal starts
    networking->doAtWiFiPortalStart([]() 
    {
        //digitalWrite(BUILTIN_LED, LOW);  // Turn on LED
        Serial.println("\r\nWiFi configuration portal started\r\n");
    });
    
    //-- Parameters: hostname, reset pin, serial object, baud rate
    debug = networking->begin("example", 0, Serial, 115200);
    
    if (!debug) 
    {
        ESP.restart(); // Restart if connection fails
    }

    #ifdef USE_ASYNC_WIFIMANAGER
    debug->println("\nAsyncWiFiManager is enabled\n");
#else
    debug->println("\nAsyncWiFiManager is disabled\n");
#endif

    //-- Define custom NTP servers (optional)
    const char* ntpServers[] = {"time.google.com", "time.cloudflare.com", nullptr};

    //-- Start NTP with Amsterdam timezone and custom servers
    if (networking->ntpStart("CET-1CEST,M3.5.0,M10.5.0/3", ntpServers))
    {
        debug->println("NTP started successfully");
    }
}

void loop() 
{
    //-- Must be called in main loop
    networking->loop();
    
    //-- Print current time every 5 seconds
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint >= 5000)
    {
      if (networking->ntpIsValid())
           showTime();
      else debug->println("NTP is not valid");
      lastPrint = millis();
    }
}
