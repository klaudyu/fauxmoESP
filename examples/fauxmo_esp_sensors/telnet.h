#ifndef TELNET_H
#define TELNET_H

/*
   Enhanced Telnet Runtime Console – ESPTelnet 2.x with Temperature Sensor Support
   ------------------------------------------------------------------------------
   Commands (type "help" after connecting):
       ping               → pong
       bye                → close session
       state  on|off      → turn LEDs on / off
       max    0-100       → set max intensity %
       min    0-100       → set min-ratio % of max
       wind   0-100       → quick fire tweak
       minfreq Hz         → set min flicker freq (0.1-20)
       maxfreq Hz         → set max flicker freq (0.1-20)
       preset <name>      → apply preset from main.cpp (roaringfire …)
       verbose on|off     → toggle extra logging
       save               → save parameters to NVS
       mem                → show heap usage
       temp               → read on-chip temperature sensor (°C)
       tempupdate         → force immediate temperature sensor update
       tempstats          → show temperature sensor statistics
       lock               → hang in a count-up loop (tests watchdog)
       logTime <1-100>    → how often to print the memory dump (seconds)
       status             → show complete system status
       help               → show this command list
*/

#include <Arduino.h>
#include <ESPTelnet.h>
#include <esp_system.h>
#include "esp_heap_caps.h"
#include "fire.h"               // FireEffect definition
#include <esp_task_wdt.h>

// ---------------------------------------------------------------------
// Forward declaration of Telnet instance (must be before helpers)
// ---------------------------------------------------------------------
static ESPTelnet telnet;                 
static constexpr uint16_t TELNET_PORT = 23;

// Temperature sensor statistics tracking
struct TempStats {
    float currentTemp = 0.0f;
    float minTemp = 999.0f;
    float maxTemp = -999.0f;
    unsigned long lastUpdate = 0;
    unsigned long updateCount = 0;
    bool isValid = false;
} tempStats;

// ---------------------------------------------------------------------
// Safe printing helpers (avoids WDT timeouts & Wi-Fi stalls)
// ---------------------------------------------------------------------
template<typename ...Args>
static void telnetSafePrintf(const char *fmt, Args... args){
    esp_task_wdt_reset();          // pre-feed watchdog
    telnet.printf(fmt, args...);   // actual send
    yield();                       // let Wi-Fi/LWIP breathe
    esp_task_wdt_reset();          // post-feed
}

#define TPRINTF(...) telnetSafePrintf(__VA_ARGS__)

// ---------------------------------------------------------------------
// External symbols coming from main.cpp
// ---------------------------------------------------------------------
extern FireEffect fire1;
extern void       save_settings();
extern void       applyPreset(const String &name);
extern bool       verbose;        // global verbosity flag in main.cpp
extern float      logTime;
extern float      readChipTemperature();  // temperature reading function
extern void       updateTemperatureSensor();  // force sensor update

// ---------------------------------------------------------------------
// Utility functions
// ---------------------------------------------------------------------
static inline bool startsWith(const String &s, const char *tok){ return s.startsWith(tok);} 
static inline float clip(float v,float lo,float hi){ return v<lo?lo:(v>hi?hi:v);} 

/**
 * Update temperature statistics with new reading
 * This function maintains running statistics about temperature readings
 * to provide insights into sensor behavior and environmental conditions
 */
static void updateTempStats(float newTemp) {
    tempStats.currentTemp = newTemp;
    tempStats.lastUpdate = millis();
    tempStats.updateCount++;
    tempStats.isValid = true;
    
    if (newTemp < tempStats.minTemp) {
        tempStats.minTemp = newTemp;
    }
    if (newTemp > tempStats.maxTemp) {
        tempStats.maxTemp = newTemp;
    }
}

/**
 * Display comprehensive system status
 * This command provides a complete overview of the fire effect controller's
 * current state, useful for monitoring and debugging
 */
static void showSystemStatus() {
    TPRINTF("\n=== Fire Effect Controller Status ===\n");
    
    // Fire effect status
    TPRINTF("Fire Effect:\n");
    //TPRINTF("  State: %s\n", fire1.get_state() ? "ON" : "OFF");
    TPRINTF("  Max Intensity: %.1f%%\n", fire1.get_intensity());
    //TPRINTF("  Current Output: %.1f%%\n", fire1.getCurrentBrightness());
    
    // Temperature sensor status
    if (tempStats.isValid) {
        TPRINTF("Temperature Sensor:\n");
        TPRINTF("  Current: %.2f°C\n", tempStats.currentTemp);
        TPRINTF("  Range: %.2f°C to %.2f°C\n", tempStats.minTemp, tempStats.maxTemp);
        TPRINTF("  Updates: %lu (last: %lu ms ago)\n", 
                tempStats.updateCount, 
                millis() - tempStats.lastUpdate);
    } else {
        TPRINTF("Temperature Sensor: No data available\n");
    }
    
    // System health
    uint32_t free = ESP.getFreeHeap();
    uint32_t bigBlk = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    TPRINTF("Memory:\n");
    TPRINTF("  Free heap: %u bytes\n", free);
    TPRINTF("  Largest block: %u bytes\n", bigBlk);
    
    // Network status
    TPRINTF("Network:\n");
    TPRINTF("  WiFi: %s\n", WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
    if (WiFi.status() == WL_CONNECTED) {
        TPRINTF("  IP: %s\n", WiFi.localIP().toString().c_str());
    }
    
    TPRINTF("Settings:\n");
    TPRINTF("  Verbose mode: %s\n", verbose ? "ON" : "OFF");
    TPRINTF("  Log interval: %.1f seconds\n", logTime);
    
    TPRINTF("=====================================\n\n");
}

// ---------------------------------------------------------------------
// Enhanced command handler with temperature sensor support
// ---------------------------------------------------------------------
static void handleCmd(String cmd){
    cmd.trim();

    // ------------------------------------------------ basic utility
    if(cmd=="ping"){ telnet.println("pong"); return; }
    if(cmd=="bye"){  telnet.println("disconnecting…"); telnet.disconnectClient(); return; }

    // ------------------------------------------------ help
    if(cmd=="help" || cmd=="?"){
        telnet.println("\n*** Fire Effect Controller Commands ***");
        telnet.println("Basic:");
        telnet.println("  ping · bye · help · status");
        telnet.println("Fire Control:");
        telnet.println("  state on|off · max <0-100> · min <0-100> · wind <0-100>");
        telnet.println("  minfreq <0.1-20> · maxfreq <0.1-20> · preset <name>");
        telnet.println("Temperature:");
        telnet.println("  temp · tempupdate · tempstats");
        telnet.println("System:");
        telnet.println("  verbose on|off · save · mem · logTime <1-100> · lock");
        telnet.println("Available presets: roaringfire, calmfire, mildfire, twinkle");
        return;
    }

    // ------------------------------------------------ system status
    if(cmd=="status"){
        showSystemStatus();
        return;
    }

    // ------------------------------------------------ verbose toggle
    if(startsWith(cmd,"verbose ")){
        verbose = cmd.substring(8).equalsIgnoreCase("on");
        TPRINTF("verbose=%s\n", verbose?"ON":"OFF");
        return;
    }

    // ------------------------------------------------ memory
    if(cmd=="mem"){
        uint32_t free   = ESP.getFreeHeap();
        uint32_t bigBlk = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
        TPRINTF("Free heap: %u bytes, largest block: %u bytes\n", free, bigBlk);
        return;
    }

    // ------------------------------------------------ temperature commands
    if(cmd=="temp"){
        float t = readChipTemperature();
        updateTempStats(t);
        TPRINTF("Chip temperature: %.2f°C\n", t);
        return;
    }

    if(cmd=="tempupdate"){
        telnet.println("Forcing temperature sensor update...");
        updateTemperatureSensor();
        float t = readChipTemperature();
        updateTempStats(t);
        TPRINTF("Temperature sensor updated: %.2f°C\n", t);
        return;
    }

    if(cmd=="tempstats"){
        if (tempStats.isValid) {
            TPRINTF("\n*** Temperature Sensor Statistics ***\n");
            TPRINTF("Current temperature: %.2f°C\n", tempStats.currentTemp);
            TPRINTF("Temperature range: %.2f°C to %.2f°C\n", tempStats.minTemp, tempStats.maxTemp);
            TPRINTF("Total updates: %lu\n", tempStats.updateCount);
            TPRINTF("Last update: %lu ms ago\n", millis() - tempStats.lastUpdate);
            TPRINTF("Temperature span: %.2f°C\n", tempStats.maxTemp - tempStats.minTemp);
            
            // Calculate average temperature rise above minimum
            float avgRise = (tempStats.currentTemp - tempStats.minTemp);
            TPRINTF("Current rise above minimum: %.2f°C\n", avgRise);
            TPRINTF("====================================\n");
        } else {
            telnet.println("No temperature data available yet. Use 'temp' command first.");
        }
        return;
    }

    // ------------------------------------------------ lock (intentional WDT trigger)
    if(cmd=="lock" || cmd=="LOCK"){
        telnet.println("Entering lock – watchdog should reset in ~10 s…");
        uint32_t sec = 0;
        while(true){
            telnet.printf("locked %u s\n", sec++);
            delay(1000);              // no WDT reset here on purpose
        }
    }

    // ------------------------------------------------ fire effect state & tuning
    if(startsWith(cmd,"state ")){
        bool on = cmd.substring(6).equalsIgnoreCase("on");
        fire1.set_state(on);
        TPRINTF("Fire effect state=%s\n", on?"ON":"OFF"); 
        return; 
    }

    if(startsWith(cmd,"max ")){
        float v = clip(cmd.substring(4).toFloat(),0,100);
        fire1.set_max_intensity(v); 
        TPRINTF("Max intensity=%.1f%%\n", v); 
        return; 
    }

    if(startsWith(cmd,"min ")){
        float v = clip(cmd.substring(4).toFloat(),0,100)/100.0f;
        fire1.set_min_ratio(v); 
        TPRINTF("Min ratio=%.2f (%.1f%%)\n", v, v*100); 
        return; 
    }
		
    if(startsWith(cmd,"logTime ")){
        logTime = clip(cmd.substring(8).toFloat(),1,100);
        TPRINTF("Memory log interval=%.1f seconds\n", logTime); 
        return; 
    }		

    if(startsWith(cmd,"wind ")){
        float v = clip(cmd.substring(5).toFloat(),0,100);
        fire1.set_wind(v); 
        TPRINTF("Wind effect=%.1f%% (affects flicker intensity and frequency)\n", v); 
        return; 
    }

    if(startsWith(cmd,"minfreq ")){
        float v = clip(cmd.substring(8).toFloat(),0.1,20);
        fire1.set_min_freq(v); 
        TPRINTF("Min flicker frequency=%.2f Hz\n", v); 
        return; 
    }

    if(startsWith(cmd,"maxfreq ")){
        float v = clip(cmd.substring(8).toFloat(),0.1,20);
        fire1.set_max_freq(v); 
        TPRINTF("Max flicker frequency=%.2f Hz\n", v); 
        return; 
    }

    if(startsWith(cmd,"preset ")){
        String p = cmd.substring(7);
        applyPreset(p); 
        TPRINTF("Applied preset '%s'\n", p.c_str()); 
        
        // Show what the preset actually changed
        telnet.println("Preset effects:");
        if (p == "roaringfire") {
            telnet.println("  → High wind (60%), intense flickering");
        } else if (p == "calmfire") {
            telnet.println("  → Low wind (20%), gentle flickering");
        } else if (p == "mildfire") {
            telnet.println("  → Medium wind (40%), balanced flickering");
        } else if (p == "twinkle") {
            telnet.println("  → Low wind (20%), subtle variations");
        } else {
            telnet.println("  → Custom preset applied");
        }
        return; 
    }

    if(cmd=="save"){ 
        save_settings(); 
        telnet.println("All settings saved to non-volatile storage"); 
        return; 
    }

    // ------------------------------------------------ unknown command
    telnet.println("Unknown command – type 'help' for available commands");
}

// --------------------------------------------------------------------- 
// Telnet connection event callbacks
// --------------------------------------------------------------------- 
static void onConnect(String ip){ 
    Serial.printf("[Telnet] %s connected\n", ip.c_str()); 
    telnet.println("=== Fire Effect Controller Console ===");
    telnet.println("Welcome! Type 'help' for commands or 'status' for system overview.");
    
    // Show quick status on connect
    if (tempStats.isValid) {
        TPRINTF("Current temperature: %.1f°C\n", tempStats.currentTemp);
    }
    //TPRINTF("Fire effect: %s\n", fire1.get_state() ? "ON" : "OFF");
}

static void onDisconnect(String ip){ 
    Serial.printf("[Telnet] %s disconnected\n", ip.c_str()); 
}

static void onReconnect(String ip){ 
    Serial.printf("[Telnet] %s reconnected\n", ip.c_str()); 
}

static void onAttempt(String ip){ 
    Serial.printf("[Telnet] %s tried to connect (busy)\n", ip.c_str()); 
}

// --------------------------------------------------------------------- 
// Setup and loop wrapper functions
// --------------------------------------------------------------------- 
inline void setupTelnet(){
    telnet.onConnect(onConnect);
    telnet.onDisconnect(onDisconnect);
    telnet.onReconnect(onReconnect);
    telnet.onConnectionAttempt(onAttempt);
    telnet.onInputReceived(handleCmd);

    Serial.print("[TELNET] Starting console service: ");
    if(telnet.begin(TELNET_PORT)) {
        Serial.println("running on port 23");
    } else { 
        Serial.println("error starting telnet – rebooting"); 
        delay(1000); 
        ESP.restart(); 
    }
}

inline void telnetLoop(){ 
    telnet.loop(); 
}

/**
 * Public function to update temperature statistics from main loop
 * Call this whenever you get a new temperature reading to maintain
 * accurate statistics for the telnet interface
 */
inline void updateTelnetTempStats(float temperature) {
    updateTempStats(temperature);
}

#endif /* TELNET_H */