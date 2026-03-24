#include <Arduino.h>
#include <WiFiManager.h>
#include "ConfigManager.h"
#include "OTAManager.h"
#include "App.h"           

ConfigManager config;
WiFiManager   wifi;
App           app;         

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== ESP32 LAYERED FIRMWARE ===");

  config.load();

  wifi.setConfigPortalTimeout(180);
  if (!wifi.autoConnect(config.getWifiApName().c_str())) {
    Serial.println("[WiFi] Failed — restarting");
    delay(3000);
    ESP.restart();
  }
  Serial.printf("[WiFi] Connected! IP: %s\n",
    WiFi.localIP().toString().c_str());

  OTAManager::begin(
    config.getDeviceName().c_str(),
    config.getOtaPassword().c_str()
  );

  app.setup();            
}

void loop() {
  OTAManager::handle();   

  app.loop();             
}