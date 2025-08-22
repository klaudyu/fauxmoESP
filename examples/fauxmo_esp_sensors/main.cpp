#define DEBUG_FAUXMO Serial
#define DEBUG_FAUXMO_VERBOSE_TCP true
#define DEBUG_FAUXMO_VERBOSE_UDP true

#include <Arduino.h>
#include "myota.h"
#include <WiFi.h>
#include "fauxmoESP.h"
#include "fire.h"
#include "telnet.h"
#include <map>
#include <vector>
#include <cmath>
#include <limits>
#include <iostream>
#include <Preferences.h>
#define PWM_NBIT 12
#define PWM_FREQ 5000
#define MAXVALUE (1<<PWM_NBIT)
#include "secrets.h"
#include <esp_wifi.h>

uint8_t newMac[] = {0xA0, 0xA2, 0x28, 0x12, 0x34, 0x16}; // Custom MAC

#define DEVNAME "mumuka"


// Global objects and variables
Preferences preferences;
FireEffect fire1("1");
fauxmoESP fauxmo;

// Device state tracking
bool last_state;
bool switch_power = false;
unsigned char value; 
unsigned int hue; 
unsigned int saturation;

// Hardware configuration for LED control
const int PWM_PIN1 = 8;       // LED output pin (GPIO 8)
const int PWM_CHANNEL1 = 0;   // PWM channel
const int PWM_FREQUENCY = 5000; // 5 kHz
const int PWM_RESOLUTION = 12;   // 12-bit resolution for smooth fire effects
const float MAX_VALUE = 1 << PWM_RESOLUTION;

// Temperature sensor configuration
unsigned long lastTempUpdate = 0;
const unsigned long TEMP_UPDATE_INTERVAL = 30000;  // Update every 30 seconds
unsigned char tempSensorId = 0;  // Will store the sensor ID for temperature
bool verbose = false;  // Global verbosity flag for telnet interface
float logTime = 20.0f;  // Memory logging interval

// FauxmoESP device IDs
unsigned char lightDeviceId = 0;

/*--------------------------------------------*/
// PRESET SYSTEM DEFINITIONS
/*--------------------------------------------*/

typedef void (*PresetFunction)();

struct PresetMode {
    std::map<String, std::pair<float, float>> modes;
    PresetFunction applyFunction;
};

/**
 * Apply default fire preset - minimal wind, baseline fire behavior
 * This represents a gentle, steady flame suitable for ambient lighting
 */
void applyDefaultPreset(){
    Serial.println("Applying default preset");
    fire1.set_wind(0);
}

/**
 * Apply roaring fire preset - high wind, dramatic flickering
 * Simulates a strong fire with lots of air movement, creating rapid intensity changes
 */
void applyRoaringFirePreset() {
    Serial.println("Applying roaring fire preset");
    fire1.set_wind(60);
}

/**
 * Apply mild fire preset - moderate wind, balanced flickering
 * Represents a healthy fire with moderate air flow and natural variation
 */
void applyMildFirePreset() {
    Serial.println("Applying mild fire preset");
    fire1.set_wind(40);
}

/**
 * Apply calm preset - low wind, gentle flickering
 * Creates a peaceful, meditative fire effect with subtle variations
 */
void applyCalmPreset() {
    Serial.println("Applying calm preset");
    fire1.set_wind(20);
}

/**
 * Apply twinkle preset - special effect with gentle variations
 * Could be enhanced to create star-like twinkling patterns
 */
void applyTwinklePreset() {
    Serial.println("Applying twinkle preset");
    fire1.set_wind(20);
}

/**
 * Preset mapping system - translates color commands from smart home systems
 * into specific fire behaviors. This allows users to say "Alexa, set the fire to red"
 * and get a roaring fire effect, or "set to blue" for twinkling.
 * 
 * The system handles both xy coordinates (from Home Assistant) and 
 * hue/saturation values (from Alexa) for maximum compatibility.
 */
std::map<String, PresetMode> presets = {
    // Red color triggers roaring fire - intuitive mapping of hot color to intense fire
    {"roaringfire", { 
        {{"xy", { 0.208,0.217}}, {"hs", {0, 254}}}, applyRoaringFirePreset
    }},
    // Yellow color triggers calm fire - warm but gentle
    {"calmfire", {
        {{"xy", {0.444,0.517}}, {"hs", {10923,254}}}, applyCalmPreset
    }},
    // Gold color triggers mild fire - in between calm and roaring
    {"mildfire", { 
        {{"xy", {0.494,0.474}}, {"hs", {9102,254}}}, applyMildFirePreset
    }},
    // Blue color triggers twinkle - cool color for special effect
    {"twinkle",{ 
        {{"xy", { 0.137,0.04}}, {"hs", {43690,254}}}, applyTwinklePreset
    }}
};

/**
 * Apply a named preset to the fire effect
 * This function provides a clean interface for both smart home commands
 * and telnet console commands to trigger predefined fire behaviors
 */
void applyPreset(const String& presetName) {
    auto it = presets.find(presetName);
    if (it != presets.end()) {
        it->second.applyFunction();
        Serial.println("Applied preset: " + presetName);
    } else {
        Serial.println("Preset not found: " + presetName);
    }
}

/**
 * Find the closest matching preset based on color coordinates
 * This algorithm calculates the distance between incoming color values
 * and predefined preset colors, selecting the best match
 * 
 * @param mode Either "xy" or "hs" indicating coordinate system
 * @param value1 X coordinate or hue value
 * @param value2 Y coordinate or saturation value
 * @return Name of closest matching preset
 */
String findClosestPreset(const String& mode, float value1, float value2) {
    String closestPreset = "";
    float minDistance = .001; // Minimum threshold for preset matching

    for (const auto& preset : presets) {
        const String& presetName = preset.first;
        const PresetMode& presetMode = preset.second;
        
        auto it = presetMode.modes.find(mode);
        if (it != presetMode.modes.end()) {
            float distance;
            if (mode == "xy") {
                // Round xy coordinates to reduce noise and improve matching
                float x1 = round(value1 * 100) / 100;
                float y1 = round(value2 * 100) / 100;
                float x2 = round(it->second.first * 100) / 100;
                float y2 = round(it->second.second * 100) / 100;
                distance = sqrt(pow(x2 - x1, 2) + pow(y2 - y1, 2));
            } else {
                // Calculate Euclidean distance in hue/saturation space
                distance = sqrt(pow(it->second.first - value1, 2) + pow(it->second.second - value2, 2));
            }

            if (distance < minDistance) {
                minDistance = distance;
                closestPreset = presetName;
            }
        }
    }

    return closestPreset;
}

/*--------------------------------------------*/
// SENSOR DISCOVERY AND DIAGNOSTICS
/*--------------------------------------------*/

/**
 * Print detailed sensor information for debugging discovery issues
 * This function helps diagnose why Home Assistant might not be finding the sensor
 */
void printSensorDebugInfo() {
    Serial.println("\n=== Sensor Discovery Debug Information ===");
    Serial.printf("Device IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("FauxmoESP Port: 80\n");
    Serial.printf("Temperature Sensor ID: %d\n", tempSensorId);
    
    // Test the sensor API endpoint
    Serial.println("\nTo manually test sensor discovery:");
    Serial.printf("1. Open browser to: http://%s/api/sensors\n", WiFi.localIP().toString().c_str());
    Serial.printf("2. Or try: http://%s/api\n", WiFi.localIP().toString().c_str());
    Serial.printf("3. In Home Assistant, try: Settings → Devices → Add Integration → Philips Hue\n");
    Serial.printf("4. Use IP: %s and any username when prompted\n", WiFi.localIP().toString().c_str());
    
    float currentTemp = readChipTemperature();
    Serial.printf("\nCurrent sensor readings:\n");
    Serial.printf("  Temperature: %.2f°C\n", currentTemp);
    Serial.printf("  Sensor active: %s\n", tempSensorId < 255 ? "Yes" : "No");
    
    Serial.println("==========================================\n");
}

/**
 * Read ESP32-C3 internal temperature sensor
 * The ESP32-C3 has a built-in temperature sensor that measures chip temperature.
 * This is useful for environmental monitoring and can provide insights into
 * device operation and ambient conditions.
 * 
 * @return Temperature in Celsius
 */
float readChipTemperature() {
    float temp = temperatureRead();
    
    if (verbose) {
        Serial.printf("[TEMP] Chip temperature: %.1f°C\n", temp);
    }
    return temp;
}

/**
 * Update the virtual temperature sensor with current reading
 * This function reads the chip temperature and updates the fauxmoESP
 * virtual sensor so that smart home systems can access the temperature data
 */
void updateTemperatureSensor() {
    float temperature = readChipTemperature();
    
    // Update the virtual temperature sensor
    fauxmo.setTemperatureC(tempSensorId, temperature, true);  // true = notify clients
    
    if (verbose) {
        Serial.printf("[SENSOR] Temperature sensor updated: %.1f°C\n", temperature);
    }
}

/*--------------------------------------------*/
// SETTINGS MANAGEMENT
/*--------------------------------------------*/

/**
 * Load all system settings from non-volatile storage
 * This includes both fire effect parameters and system configuration
 */
void load_settings(){
    fire1.load_settings();
    // Could add temperature sensor settings here if needed
    Serial.println("[SETTINGS] All settings loaded from NVS");
}

/**
 * Save all system settings to non-volatile storage
 * Ensures persistence across power cycles and system restarts
 */
void save_settings(){
    fire1.save_settings();
    // Could add temperature sensor settings here if needed
    Serial.println("[SETTINGS] All settings saved to NVS");
} 

/*--------------------------------------------*/
// MAIN SETUP FUNCTION
/*--------------------------------------------*/


void setup() {
	Serial.begin(115200);
	WiFi.mode(WIFI_STA);
    delay(500); 
	esp_err_t macChangeResult=esp_wifi_set_mac(WIFI_IF_STA, &newMac[0]);
	if (macChangeResult == ESP_OK){
		Serial.println("mac address changed successfully");
	}else{ Serial.println("ERROR changing mac address"); };

    delay(500); // Allow system to stabilize after boot

    // Configure PWM for LED control with high resolution for smooth fire effects
    ledcSetup(PWM_CHANNEL1, PWM_FREQUENCY, PWM_RESOLUTION);
    ledcAttachPin(PWM_PIN1, PWM_CHANNEL1);

    // Start with LED off
    ledcWrite(PWM_CHANNEL1, 0);

    //Serial.begin(115200);
    Serial.println("\n=== Enhanced Fire Effect Controller Starting ===");

    // Initialize preferences system for persistent storage
    preferences.begin(DEVNAME, false);

    /*--------------------------------------------*/
    // WiFi Connection Setup
    /*--------------------------------------------*/
    Serial.printf("[WIFI] Connecting to %s", ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    Serial.println();
    Serial.println("[WIFI] Connected to WiFi");
    Serial.print("[WIFI] IP Address: ");
    Serial.println(WiFi.localIP());

    /*--------------------------------------------*/
    // OTA Update Setup
    /*--------------------------------------------*/
    otasetup(DEVNAME); // Note: MDNS name will be overridden by fauxmo later
    Serial.println("[OTA] Over-the-air update service initialized");

    /*--------------------------------------------*/
    // FauxmoESP Virtual Device Setup
    /*--------------------------------------------*/
    Serial.println("[FAUXMO] Configuring virtual devices...");
    
    // Add virtual light device for fire effect control
    lightDeviceId = fauxmo.addDevice(DEVNAME);
    Serial.printf("[FAUXMO] Added virtual light: '%s' (ID: %d)\n", DEVNAME, lightDeviceId);
    
    // Add virtual temperature sensor for environmental monitoring
    String tempSensorName = String(DEVNAME) + "_temp";
    tempSensorId = fauxmo.addTemperatureSensor(tempSensorName.c_str());
    Serial.printf("[FAUXMO] Added temperature sensor: '%s' (ID: %d)\n", tempSensorName.c_str(), tempSensorId);

    // Take initial temperature reading and initialize sensor with valid data
    // This ensures the sensor has real data before Home Assistant tries to discover it
    Serial.println("[FAUXMO] Initializing temperature sensor with current reading...");
    float initialTemp = readChipTemperature();
    fauxmo.setTemperatureC(tempSensorId, initialTemp, false);  // false = don't notify yet
    Serial.printf("[FAUXMO] Temperature sensor initialized with %.1f°C\n", initialTemp);

    // Configure fauxmo service parameters
    fauxmo.setPort(80);
    fauxmo.enableMDNS(DEVNAME); // This overrides the OTA MDNS name
    
    // Small delay to ensure all devices are fully configured before enabling
    delay(1000);
    
    fauxmo.enable(true);
    Serial.println("[FAUXMO] FauxmoESP service enabled - devices ready for discovery");

    /*--------------------------------------------*/
    // Smart Home Command Callback Setup
    /*--------------------------------------------*/
    
    /**
     * Main callback for smart home system commands
     * This function translates commands from Alexa, Google Home, Home Assistant, etc.
     * into fire effect parameters and preset activations
     * 
     * The callback receives color information, brightness, and state commands
     * and maps them to appropriate fire behaviors through the preset system
     */
    fauxmo.onSetState([](unsigned char device_id, const char * device_name, bool state, 
                          unsigned char _value, unsigned int _hue, unsigned int _saturation, 
                          unsigned int ct) {
        
        // Convert brightness from 0-255 to 0-100 percentage
        value = _value / 255.0f * 100;
        
        // Update fire effect state and intensity
        fire1.set_state(state);
        fire1.set_max_intensity(value);

        // Get current color mode from the fauxmo device
        char colormode[3];
        fauxmo.getColormode(device_id, colormode, 3);
        
        // Get xy coordinates for preset matching
        float _x = fauxmo.getX(device_id);
        float _y = fauxmo.getY(device_id);

        Serial.println("========================================");
        Serial.printf("[FAUXMO] Device: %s, State: %s, Brightness: %.1f%%\n", 
                     device_name, state ? "ON" : "OFF", value);
        Serial.printf("[FAUXMO] Color mode: %s\n", colormode);
        Serial.printf("[FAUXMO] HSV: %d %d %.1f, XY: %.3f %.3f\n", 
                     _hue, _saturation, value, _x, _y);

        // Find and apply the closest matching preset based on color
        String closestPreset;
        if (strcmp(colormode, "xy") == 0) {
            closestPreset = findClosestPreset("xy", _x, _y);
        } else if (strcmp(colormode, "hs") == 0) {
            closestPreset = findClosestPreset("hs", _hue, _saturation);
        } else {
            Serial.println("[FAUXMO] Unsupported color mode, using default");
        }

        // Apply the matched preset or default behavior
        if (closestPreset.length() > 0) {
            applyPreset(closestPreset);
            Serial.printf("[FAUXMO] Applied preset: %s\n", closestPreset.c_str());
        } else {
            Serial.println("[FAUXMO] No matching preset found, using default");
            applyDefaultPreset();
        }

        // Save settings to preserve state across restarts
        save_settings();
        Serial.println("========================================");
    });

    /*--------------------------------------------*/
    // Telnet Console Setup
    /*--------------------------------------------*/
    setupTelnet();
    Serial.println("[TELNET] Console interface initialized on port 23");

    /*--------------------------------------------*/
    // Load Saved Settings and Complete Sensor Initialization
    /*--------------------------------------------*/
    load_settings();
    Serial.println("[SETUP] Loading saved fire effect settings...");
    
    // Perform initial temperature reading and complete sensor setup
    Serial.println("[SETUP] Completing temperature sensor initialization...");
    updateTemperatureSensor();
    
    // Print comprehensive sensor debug information
    printSensorDebugInfo();
    
    Serial.println("[SETUP] Initial temperature reading completed");

    /*--------------------------------------------*/
    // Setup Complete
    /*--------------------------------------------*/
    Serial.println("\n=== Setup Complete ===");
    Serial.printf("Fire Effect Controller '%s' is now active:\n", DEVNAME);
    Serial.printf("  • Light Control: '%s'\n", DEVNAME);
    Serial.printf("  • Temperature Sensor: '%s_temp'\n", DEVNAME);
    Serial.printf("  • IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("  • Telnet Console: %s:23\n", WiFi.localIP().toString().c_str());
    Serial.println("  • Available Presets: roaringfire, calmfire, mildfire, twinkle");
    Serial.println("  • Voice Commands: 'Alexa, turn on marry', 'Set marry to red'");
    Serial.println("========================================\n");
}

/*--------------------------------------------*/
// WIFI CONNECTION MANAGEMENT
/*--------------------------------------------*/

bool shouldAttemptReconnect = false;
unsigned long lastAttemptTime = 0;
unsigned long firstAttemptTime = 0;  

/**
 * Monitor WiFi connection and handle automatic reconnection
 * This function implements a robust reconnection strategy that prevents
 * infinite retry loops while ensuring connectivity is restored when possible
 */
void check_reboot_wifi(){
    if (WiFi.status() != WL_CONNECTED) {
        if (!shouldAttemptReconnect) {
            Serial.println("[WIFI] Lost connection. Will try to reconnect...");
            shouldAttemptReconnect = true;
            lastAttemptTime = millis();
            firstAttemptTime = millis();
        }

        if (shouldAttemptReconnect && millis() - lastAttemptTime > 5000) {  // Attempt every 5 seconds
            Serial.println("[WIFI] Attempting to reconnect...");
            WiFi.begin(ssid, password);
            lastAttemptTime = millis();
            
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("[WIFI] Reconnected successfully!");
                shouldAttemptReconnect = false;
            }
        }
    } else {
        // Reset reconnection state when connected
        shouldAttemptReconnect = false;
    }
    
    // If reconnection failed for too long, restart the system
    if (shouldAttemptReconnect && millis() - firstAttemptTime > 30000) {  // 30 seconds timeout
        Serial.println("[WIFI] Reconnect failed after 30 seconds. Rebooting...");
        ESP.restart();
    }
}

/*--------------------------------------------*/
// LED CONTROL AND MEMORY MONITORING
/*--------------------------------------------*/

/**
 * Write brightness value to LED using PWM
 * Converts percentage brightness to PWM duty cycle for smooth fire effects
 * 
 * @param perc1 Brightness percentage (0-100)
 */
void writeled(float perc1){
    ledcWrite(PWM_CHANNEL1, (int)( (100-perc1) * MAX_VALUE / 100)); 
}

#include <esp_system.h>

/**
 * Log memory usage for system health monitoring
 * Helps detect memory leaks and optimize system performance
 */
void logMemoryUsage() {
    static unsigned long lastLog = 0;
    if (millis() - lastLog > logTime * 1000) {  // Convert seconds to milliseconds
        Serial.printf("[MEMORY] Free Heap: %d, Largest Free Block: %d\n", 
            ESP.getFreeHeap(), 
            heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        lastLog = millis();
    }
}

/*--------------------------------------------*/
// MAIN LOOP
/*--------------------------------------------*/

void loop() {
    /*--------------------------------------------*/
    // Core System Services
    /*--------------------------------------------*/
    
    // Monitor and maintain WiFi connection
    check_reboot_wifi();
    
    // Update fire effect and write to LED
    // The fire effect engine calculates realistic flickering patterns
    writeled(fire1.update());
    
    /*--------------------------------------------*/
    // Periodic Tasks
    /*--------------------------------------------*/
    
    // Log memory usage for system health monitoring
    logMemoryUsage();
    
    // Update temperature sensor periodically
    if (millis() - lastTempUpdate > TEMP_UPDATE_INTERVAL) {
        lastTempUpdate = millis();
        updateTemperatureSensor();
    }
    
    /*--------------------------------------------*/
    // Network Services
    /*--------------------------------------------*/
    
    // Handle OTA update requests
    ArduinoOTA.handle();
    
    // Process smart home system requests
    fauxmo.handle();
    
    // Handle telnet console connections and commands
    telnetLoop();
    
    // Small delay for system stability and power efficiency
    delay(10);
}