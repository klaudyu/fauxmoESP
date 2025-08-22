#ifndef __my_ota_
#define __my_ota_

#include <ArduinoOTA.h>

void otasetup(const char*mdnsname){
  // Initialize OTA
  ArduinoOTA.setHostname(mdnsname);

  ArduinoOTA
    .onStart([]() {
      Serial.println("Start"); })
    .onEnd([]() {
      Serial.println("\nEnd"); })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100))); })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error); });

  ArduinoOTA.begin();
}

#endif
