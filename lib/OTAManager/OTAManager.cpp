#include "OTAManager.h"

void OTAManager::begin(const char* hostname, const char* password) {
  if (MDNS.begin(hostname)) {
    Serial.printf("[OTA] mDNS: %s.local ready\n", hostname);
  }

  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.setPassword(password);

  ArduinoOTA.onStart([]() {
    Serial.println("[OTA] Starting...");
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("[OTA] Done! Rebooting...");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("[OTA] %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[OTA] Error[%u]: ", error);
    if      (error == OTA_AUTH_ERROR)    Serial.println("Auth failed");
    else if (error == OTA_BEGIN_ERROR)   Serial.println("Begin failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive failed");
    else if (error == OTA_END_ERROR)     Serial.println("End failed");
  });

  ArduinoOTA.begin();
  Serial.printf("[OTA] Ready! IP: %s\n",
    WiFi.localIP().toString().c_str());
}

void OTAManager::handle() {
  ArduinoOTA.handle();
}