#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>

class ConfigManager {
public:
  bool load();

  String getDeviceName();
  String getOtaPassword();
  String getWifiApName();

private:
  String _deviceName  = "esp32-device";  
  String _otaPassword = "admin123";
  String _wifiApName  = "ESP32-Setup";
};