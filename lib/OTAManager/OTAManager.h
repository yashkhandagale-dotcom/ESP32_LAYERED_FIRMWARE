#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>

class OTAManager {
public:
  static void begin(const char* hostname,
                    const char* password);
  static void handle();
};