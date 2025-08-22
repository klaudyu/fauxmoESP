#pragma once

// All format strings live in PROGMEM to save RAM.
// These templates are consumed with snprintf_P in the .cpp.

PROGMEM const char FAUXMO_TCP_HEADERS[] =
    "HTTP/1.1 %s\r\n"
    "Content-Type: %s\r\n"
    "Content-Length: %u\r\n"   // was %d; safer to print an unsigned length
    "Connection: close\r\n\r\n";

// State response: array of success objects for the properties we updated.
PROGMEM const char FAUXMO_TCP_STATE_RESPONSE[] = "["
    "{\"success\":{\"/lights/%d/state/on\":%s}},"
    "{\"success\":{\"/lights/%d/state/bri\":%d}},"   // present for compatibility
    "{\"success\":{\"/lights/%d/state/hue\":%d}},"
    "{\"success\":{\"/lights/%d/state/sat\":%d}},"
    "{\"success\":{\"/lights/%d/state/ct\":%d}},"
    "{\"success\":{\"/lights/%d/state/xy\":[%.3f,%.3f]}}"
"]";

// Detailed device description (used for single device queries)
PROGMEM const char FAUXMO_DEVICE_JSON_TEMPLATE[] = "{"
    "\"type\": \"Extended color light\","
    "\"name\": \"%s\","
    "\"uniqueid\": \"%s\","
    "\"modelid\": \"LLC020\","
    "\"manufacturername\": \"Signify Netherlands B.V.\","
    "\"productname\": \"Hue go\","
    "\"state\":{"
        "\"on\": %s,"
        "\"bri\": %d,"
        "\"xy\": [%.3f,%.3f],"
        "\"colormode\": \"%s\","
        "\"hue\": %d,"
        "\"sat\": %d,"
        "\"effect\": \"none\","
        "\"ct\": %d,"
        "\"mode\": \"homeautomation\","
        "\"reachable\": true"
    "},"
    "\"capabilities\": {"
        "\"certified\": true,"
        "\"streaming\": {\"renderer\":true,\"proxy\":false}"
        // Optional: R"(,"effects":["samba","dance","candle"])"
    "},"
    "\"swversion\": \"67.116.3\""
"}";

// Short device description (used for lists)
PROGMEM const char FAUXMO_DEVICE_JSON_TEMPLATE_SHORT[] = "{"
    "\"type\": \"Extended color light\","
    "\"name\": \"%s\","
    "\"uniqueid\": \"%s\""
"}";

// Bridge description (UPnP device description document)
PROGMEM const char FAUXMO_DESCRIPTION_TEMPLATE[] =
"<?xml version=\"1.0\" ?>"
"<root xmlns=\"urn:schemas-upnp-org:device-1-0\">"
    "<specVersion><major>1</major><minor>0</minor></specVersion>"
    "<URLBase>http://%d.%d.%d.%d:%d/</URLBase>"
    "<device>"
        "<deviceType>urn:schemas-upnp-org:device:Basic:1</deviceType>"
        "<friendlyName>Philips hue (%d.%d.%d.%d:%d)</friendlyName>"
        "<manufacturer>Royal Philips Electronics</manufacturer>"
        "<manufacturerURL>http://www.philips.com</manufacturerURL>"
        "<modelDescription>Philips hue Personal Wireless Lighting</modelDescription>"
        "<modelName>Philips hue bridge 2012</modelName>"
        "<modelNumber>929000226503</modelNumber>"
        "<modelURL>http://www.meethue.com</modelURL>"
        "<serialNumber>%s</serialNumber>"
        "<UDN>uuid:2f402f80-da50-11e1-9b23-%s</UDN>"
        "<presentationURL>index.html</presentationURL>"
    "</device>"
"</root>";

PROGMEM const char FAUXMO_UDP_RESPONSE_TEMPLATE[] =
    "HTTP/1.1 200 OK\r\n"
    "EXT:\r\n"
    "CACHE-CONTROL: max-age=100\r\n"  // SSDP_INTERVAL
    "LOCATION: http://%d.%d.%d.%d:%d/description.xml\r\n"
    "SERVER: FreeRTOS/6.0.5, UPnP/1.0, IpBridge/1.17.0\r\n"
    "hue-bridgeid: %s\r\n"
    "ST: urn:schemas-upnp-org:device:basic:1\r\n"
    "USN: uuid:2f402f80-da50-11e1-9b23-%s::upnp:rootdevice\r\n"
    "\r\n";
