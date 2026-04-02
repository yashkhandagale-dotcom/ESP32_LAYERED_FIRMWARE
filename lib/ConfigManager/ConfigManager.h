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
  String _deviceName  = "plc-001";  
  String _otaPassword = "factory2024";
  String _wifiApName  = "ESP32-Setup";
};