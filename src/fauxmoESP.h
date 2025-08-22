/*

FAUXMO ESP

Copyright (C) 2016-2020 by Xose PÃ©rez <xose dot perez at gmail dot com>, 2020-2021 by Paul Vint <paul@vintlabs.com>

The MIT License (MIT)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*/

#pragma once

#define FAUXMO_UDP_MULTICAST_IP     IPAddress(239,255,255,250)
#define FAUXMO_UDP_MULTICAST_PORT   1900
#define FAUXMO_TCP_MAX_CLIENTS      10
#define FAUXMO_TCP_PORT             1905
#define FAUXMO_RX_TIMEOUT           3
#define FAUXMO_DEVICE_UNIQUE_ID_LENGTH  27

#if defined(ESP8266)
    #include <ESP8266mDNS.h>
#elif defined(ESP32)
    #include <ESPmDNS.h>
#elif defined(ARDUINO_RASPBERRY_PI_PICO_W)
    #include <LEAmDNS.h>
#else
    #error Platform not supported
#endif

#define DEBUG_FAUXMO                Serial

#ifdef DEBUG_FAUXMO
    #if defined(ARDUINO_ARCH_ESP32)
        #define DEBUG_MSG_FAUXMO(fmt, ...) { DEBUG_FAUXMO.printf_P((PGM_P) PSTR(fmt), ## __VA_ARGS__); }
    #else
        #define DEBUG_MSG_FAUXMO(fmt, ...) { DEBUG_FAUXMO.printf(fmt, ## __VA_ARGS__); }
    #endif
#else
    #define DEBUG_MSG_FAUXMO(...)
#endif

#ifndef DEBUG_FAUXMO_VERBOSE_TCP
#define DEBUG_FAUXMO_VERBOSE_TCP    true
#endif

#ifndef DEBUG_FAUXMO_VERBOSE_UDP
#define DEBUG_FAUXMO_VERBOSE_UDP    true
#endif

#include <Arduino.h>

#if defined(ESP8266)
    #include <ESP8266WiFi.h>
    #include <ESPAsyncTCP.h>
#elif defined(ESP32)
    #include <WiFi.h>
    #include <AsyncTCP.h>
#elif defined(ARDUINO_RASPBERRY_PI_PICO_W)
    #include <AsyncTCP_RP2040W.h>
#else
	#error Platform not supported
#endif

#include <WiFiUdp.h>
#include <functional>
#include <vector>
#include <MD5Builder.h>
#include "templates.h"



typedef struct {
    char * name;
    bool state;
    unsigned char value;
    char uniqueid[FAUXMO_DEVICE_UNIQUE_ID_LENGTH];
	unsigned int hue;
    unsigned int saturation;
    unsigned int ct;
    float x,y;
    char colormode[3];  // This might have to change to an enum 
    unsigned char red, green, blue;
} fauxmoesp_device_t;

// --- Sensor support ---
typedef enum : uint8_t {
    SENSOR_PRESENCE,
    SENSOR_TEMPERATURE,
    SENSOR_LIGHTLEVEL
} fauxmo_sensor_type_t;

typedef struct {
    char *name;
    fauxmo_sensor_type_t type;
    char uniqueid[FAUXMO_DEVICE_UNIQUE_ID_LENGTH];
    bool reachable;
    // state fields
    bool presence;
    int32_t temperature;   // centi-degrees C
    uint32_t lightlevel;   // 0..65535
} fauxmoesp_sensor_t;


typedef std::function<void(unsigned char, const char *, bool, unsigned char, unsigned int, unsigned int, unsigned int)> TSetStateCallback;
typedef std::function<void(unsigned char, const fauxmoesp_device_t*)> TSetStateCallbackObject;

class fauxmoESP {

    public:

        ~fauxmoESP();

        unsigned char addDevice(const char * device_name);
        bool renameDevice(unsigned char id, const char * device_name);
        bool renameDevice(const char * old_device_name, const char * new_device_name);
        bool removeDevice(unsigned char id);
        bool removeDevice(const char * device_name);
        char * getDeviceName(unsigned char id, char * buffer, size_t len);
        int getDeviceId(const char * device_name);
        const fauxmoesp_device_t* getDevice(unsigned char id) const ;
        void setDeviceUniqueId(unsigned char id, const char *uniqueid);
        void onSetState(TSetStateCallback fn) { _setCallback = fn; _setCallbackObject = nullptr; }
        void onSetState(TSetStateCallbackObject fn) { _setCallbackObject = fn; _setCallback = nullptr; }
        bool setState(unsigned char id, bool state, unsigned char value);
        bool setState(const char * device_name, bool state, unsigned char value);
		
		bool setState(unsigned char id, bool state, unsigned int hue, unsigned int saturation);
        bool setState(const char * device_name, bool state, unsigned int hue, unsigned int saturation);
        bool setState(unsigned char id, bool state, unsigned int ct);
        bool setState(const char * device_name, bool state, unsigned int ct);
		bool setState(unsigned char id, bool state);
        bool setState(const char * device_name, bool state);

        uint8_t getRed(unsigned char id) { return id < _devices.size() ? _devices[id].red : 0; }
        uint8_t getGreen(unsigned char id) { return id < _devices.size() ? _devices[id].green : 0; }
        uint8_t getBlue(unsigned char id) { return id < _devices.size() ? _devices[id].blue : 0; }

        unsigned char getBrightness(unsigned char id) { return id < _devices.size() ? _devices[id].value : 0; }
        float getX(unsigned char id) { return id < _devices.size() ? _devices[id].x : 0.0f; }
        float getY(unsigned char id) { return id < _devices.size() ? _devices[id].y : 0.0f; }

        char * getColormode(unsigned char id, char * buffer, size_t len);
		
        bool process(AsyncClient *client, bool isGet, String url, String body);
        void enable(bool enable);
        void createServer(bool internal) { _internal = internal; }
        void setPort(unsigned long tcp_port) { _tcp_port = tcp_port; }
        void handle();
        void enableMDNS(const char *name);
		// notify connected clients about a light state (by id or by name)
		void notifyState(unsigned char id);
		void notifyState(const char * device_name);

    
        // --- Sensors API ---
        unsigned char addPresenceSensor(const char *name);
        unsigned char addTemperatureSensor(const char *name);
        unsigned char addLightLevelSensor(const char *name);
        void setPresence(unsigned char id, bool present, bool notify = true);
        void setTemperatureC(unsigned char id, float celsius, bool notify = true);
        void setLightLevel(unsigned char id, uint32_t level, bool notify = true);
        void notifySensor(unsigned char id);

    private:

        String _tcpBuffers[FAUXMO_TCP_MAX_CLIENTS];
        AsyncServer * _server = nullptr;
        bool _enabled = false;
        bool _internal = true;
        unsigned int _tcp_port = FAUXMO_TCP_PORT;
        std::vector<fauxmoesp_device_t> _devices;
		#ifdef ESP8266
        WiFiEventHandler _handler;
		#endif
        WiFiUDP _udp;
        AsyncClient * _tcpClients[FAUXMO_TCP_MAX_CLIENTS] = {nullptr};
        TSetStateCallback _setCallback = nullptr;
        TSetStateCallbackObject _setCallbackObject = nullptr;

        String _deviceJson(unsigned char id, bool all = true); 	// all = true means we are listing all devices so use full description template

        String bridgeid; 
		
		void _setRGBFromHSV(unsigned char id);
        void _adjustRGBFromValue(unsigned char id);
        void _setRGBFromCT(unsigned char id);
        void _setRGBFromXY(unsigned char id);
        void _setHSVFromRGB(unsigned char id);

        void _handleUDP();
        void _onUDPData(const IPAddress remoteIP, unsigned int remotePort, void *data, size_t len);
        void _sendUDPResponse();

        void _onTCPClient(AsyncClient *client);
        bool _onTCPData(AsyncClient *client, void *data, size_t len);
        bool _onTCPRequest(AsyncClient *client, bool isGet, String url, String body);
        bool _onTCPDescription(AsyncClient *client, String url, String body);
        bool _onTCPList(AsyncClient *client, String url, String body);
        bool _onTCPControl(AsyncClient *client, String url, String body);
        void _sendTCPResponse(AsyncClient *client, const char * code, char * body, const char * mime);
        String _listLightsJson(unsigned char id = 0) ;
        String _listConfig() ;
        String _listGroups() ;
        String _getLightStateJson(unsigned char id);

        
        // Sensors storage and helpers
        String _sensorsJson(unsigned char id);
        const char* _sensorTypeName(fauxmo_sensor_type_t t);
        String _sensorStateJson(const fauxmoesp_sensor_t& s);
        std::vector<fauxmoesp_sensor_t> _sensors;
        String _byte2hex(uint8_t zahl);
        String _makeMD5(String text);
        const char *_mdns_name = nullptr;
        void _startMDNS();
};