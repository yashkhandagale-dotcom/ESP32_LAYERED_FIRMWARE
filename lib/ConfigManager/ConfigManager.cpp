#include "ConfigManager.h"

bool ConfigManager::load() {
  if (!SPIFFS.begin(true)) {
    Serial.println("[Config] SPIFFS mount failed!");
    return false;
  }

  if (!SPIFFS.exists("/config.json")) {
    Serial.println("[Config] config.json not found! Using defaults.");
    return false;
  }

  File file = SPIFFS.open("/config.json", "r");
  if (!file) {
    Serial.println("[Config] Failed to open config.json!");
    return false;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.printf("[Config] JSON parse failed: %s\n", error.c_str());
    return false;
  }

  _deviceName  = doc["device_name"]  | _deviceName;
  _otaPassword = doc["ota_password"] | _otaPassword;
  _wifiApName  = doc["wifi_ap_name"] | _wifiApName;

  Serial.println("[Config] Loaded successfully!");
  Serial.printf("[Config] Device: %s\n", _deviceName.c_str());

  return true;
}

String ConfigManager::getDeviceName()  { return _deviceName;  }
String ConfigManager::getOtaPassword() { return _otaPassword; }
String ConfigManager::getWifiApName()  { return _wifiApName;  }