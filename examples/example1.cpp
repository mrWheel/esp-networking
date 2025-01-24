#include "Networking.h"

Networking* networking = nullptr;
Stream* debug = nullptr;

uint32_t loopCount = 0;

//-- OTA callback functions
void onOTAStart()
{
    debug->println("Custom OTA Start Handler: Preparing for update...");
}

void onOTAProgress()
{
    debug->println("Custom OTA Progress Handler: Another 10% completed");
}

void onOTAEnd()
{
    debug->println("Custom OTA End Handler: Update process finishing...");
}

void setup() 
{
    delay(5000);
    
    networking = new Networking();
    #ifdef ESP8266
        debug = networking->begin("esp8266", 0, Serial, 115200);
    #else
        debug = networking->begin("esp32", 0, Serial, 115200);
    #endif
    
    if (!debug) 
    {
        //-- if connection fails .. restart
        ESP.restart();
    }
    
    //-- Register OTA callbacks
    networking->doAtStartOTA(onOTAStart);
    //networking->doAtProgressOTA(onOTAProgress);
    networking->doAtEndOTA(onOTAEnd);
    
    //-- Example of using the IP methods
    if (networking->isConnected()) 
    {
        debug->print("Device IP: ");
        debug->println(networking->getIPAddressString());
    }
}

void loop() 
{
    networking->loop();
    
    //-- Your main code here
    delay(1000);
    debug->printf("Hello, world! IP: %s, loopCount[%d]\n",
                 networking->getIPAddressString().c_str(),
                 loopCount++);
    
    //-- Example: Periodically check connection status
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 30000) //-- Every 30 seconds
    {  
        if (networking->isConnected()) 
        {
            debug->print("Still connected. IP: ");
            debug->println(networking->getIPAddressString());
        } 
        else 
        {
            debug->println("WiFi connection lost!");
        }
        lastCheck = millis();
    }
}
