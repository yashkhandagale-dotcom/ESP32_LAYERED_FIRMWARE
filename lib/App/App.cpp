#include "App.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ModbusIP_ESP8266.h>
#include <ModbusRTU.h>

WiFiUDP udp;
const uint16_t DISCOVERY_PORT = 8888;

#define MAX_GATEWAYS 5
struct Gateway { String ip; uint16_t port; };
Gateway gateways[MAX_GATEWAYS];
int gatewayCount = 0;

WiFiClient opcClients[MAX_GATEWAYS];
bool opcConnected[MAX_GATEWAYS] = {};

ModbusIP mbTcp;
ModbusRTU mbRtu;

const uint8_t RTU_SLAVE_ID = 1;
const uint32_t RTU_BAUD = 9600;
const uint8_t PIN_RS485_DE = 4;

const uint16_t REG_VOLTAGE = 0;
const uint16_t REG_TEMPERATURE = 4;

#define TEMP_PIN 14
#define ROT_CLK  18
#define ROT_DT   19

OneWire oneWire(TEMP_PIN);
DallasTemperature sensors(&oneWire);

volatile int encoderCount = 0;
volatile int lastEncoded = 0;

float g_voltage = 24.0f;
float g_temperature = 25.0f;

enum DiscoveryState { DISC_IDLE, DISC_SEND, DISC_WAIT };
DiscoveryState discState = DISC_IDLE;

unsigned long discLastAction = 0;
int discAttempt = 0;

const unsigned long DISC_INTERVAL = 10000;
const unsigned long DISC_WAIT_TIME = 1000;
const int DISC_MAX_ATTEMPTS = 5;

void IRAM_ATTR readEncoder() {
    int MSB = digitalRead(ROT_CLK);
    int LSB = digitalRead(ROT_DT);
    int encoded = (MSB << 1) | LSB;
    int sum = (lastEncoded << 2) | encoded;

    if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderCount++;
    if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderCount--;

    lastEncoded = encoded;
}

union FloatRegs { float f; uint16_t w[2]; };

void writeFloat(uint16_t startReg, float value) {
    FloatRegs d; d.f = value;
    mbTcp.Hreg(startReg, d.w[1]);
    mbTcp.Hreg(startReg+1, d.w[0]);
    mbRtu.Hreg(startReg, d.w[1]);
    mbRtu.Hreg(startReg+1, d.w[0]);
}

void handleDiscovery() {

    switch (discState) {

        case DISC_IDLE:
            if (gatewayCount == 0 && millis() - discLastAction > DISC_INTERVAL) {
                Serial.println("[Discovery] No gateways, starting discovery...");
                udp.begin(DISCOVERY_PORT);
                discAttempt = 0;
                discState = DISC_SEND;
            }
            break;

        case DISC_SEND:
            if (discAttempt < DISC_MAX_ATTEMPTS) {
                Serial.printf("[Discovery] Sending WHO_IS_GATEWAY? attempt %d\n", discAttempt+1);
                udp.beginPacket("255.255.255.255", DISCOVERY_PORT);
                udp.print("WHO_IS_GATEWAY?");
                udp.endPacket();

                discLastAction = millis();
                discState = DISC_WAIT;
            } else {
                Serial.println("[Discovery] Max attempts reached, retry later");
                discState = DISC_IDLE;
                discLastAction = millis();
            }
            break;

        case DISC_WAIT:

            if (udp.parsePacket()) {
                char buf[128];
                int len = udp.read(buf, sizeof(buf) - 1);
                buf[len] = 0;

                String reply(buf);
                Serial.printf("[Discovery] Received reply: %s\n", reply.c_str());

                if (reply.startsWith("I_AM_GATEWAY")) {
                    int p1 = reply.indexOf(":");
                    int p2 = reply.lastIndexOf(":");
                    if (p1 > 0 && p2 > p1) {
                        String ip = reply.substring(p1 + 1, p2);
                        uint16_t port = reply.substring(p2 + 1).toInt();
                        gateways[0] = { ip, port };
                        gatewayCount = 1;
                        Serial.printf("[Discovery] Gateway found: %s:%d\n", ip.c_str(), port);
                    }
                }

                discState = DISC_IDLE;
                discLastAction = millis();
                break;
            }

            if (millis() - discLastAction > DISC_WAIT_TIME) {
                discAttempt++;
                discState = DISC_SEND;
            }
            break;
    }
}

void connectToOpcServers() {

    handleDiscovery();

    if (gatewayCount == 0) return;

    for(int i=0;i<gatewayCount;i++){

        if(opcClients[i].connected()){
            opcConnected[i] = true;
            continue;
        }

        opcConnected[i] = opcClients[i].connect(gateways[i].ip.c_str(), gateways[i].port);

        if(opcConnected[i])
            Serial.printf("[OPC] Connected to %s:%d\n", gateways[i].ip.c_str(), gateways[i].port);
        else
            Serial.printf("[OPC] Failed to connect to %s:%d\n", gateways[i].ip.c_str(), gateways[i].port);
    }
}

void sendOpcData(float voltage, float temperature){

    StaticJsonDocument<256> doc;
    doc["deviceId"]="ESP32_01";
    doc["timestamp"]=millis();
    doc["ns=2;s=Plant=MUMBAI_PLANT/Line=ASSEMBLY_01/Machine=CNC_02/Signal=VOLTAGE"]=voltage;
    doc["ns=2;s=Plant=MUMBAI_PLANT/Line=ASSEMBLY_01/Machine=CNC_02/Signal=TEMP"]=temperature;

    char buf[256];
    size_t len = serializeJson(doc, buf);

    for(int i=0;i<gatewayCount;i++)
        if(opcConnected[i] && opcClients[i].connected()) {
            opcClients[i].write((uint8_t*)buf,len);
            opcClients[i].write('\n');
            Serial.printf("[OPC] Sent data to %s:%d -> V=%.2f T=%.2f\n",
                gateways[i].ip.c_str(), gateways[i].port, voltage, temperature);
        }
}

void App::setup() {

    Serial.println("[App] Setup");

    pinMode(ROT_CLK, INPUT_PULLUP);
    pinMode(ROT_DT, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(ROT_CLK), readEncoder, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ROT_DT), readEncoder, CHANGE);

    sensors.begin();

    mbTcp.server();
    mbTcp.addHreg(REG_VOLTAGE);
    mbTcp.addHreg(REG_VOLTAGE+1);
    mbTcp.addHreg(REG_TEMPERATURE);
    mbTcp.addHreg(REG_TEMPERATURE+1);

    Serial2.begin(RTU_BAUD,SERIAL_8N1,16,17);
    pinMode(PIN_RS485_DE, OUTPUT);
    digitalWrite(PIN_RS485_DE, LOW);

    mbRtu.begin(&Serial2,PIN_RS485_DE);
    mbRtu.slave(RTU_SLAVE_ID);

    mbRtu.addHreg(REG_VOLTAGE);
    mbRtu.addHreg(REG_VOLTAGE+1);
    mbRtu.addHreg(REG_TEMPERATURE);
    mbRtu.addHreg(REG_TEMPERATURE+1);

    Serial.println("[App] Setup complete");
}

void App::loop() {

    static unsigned long lastTempMs = 0;
    static unsigned long lastOpcMs  = 0;
    static unsigned long lastLoopMs = 0;
    static int lastCount = 0;

    mbRtu.task();
    mbTcp.task();
    connectToOpcServers();

    if(millis() - lastLoopMs >= 1000){
        lastLoopMs = millis();
        Serial.println("[App] Loop iteration");
    }

    int diff = encoderCount - lastCount;
    if(diff!=0){
        g_voltage += diff*0.1f;
        lastCount = encoderCount;
        Serial.printf("[Encoder] Count=%d Voltage=%.2f\n", encoderCount, g_voltage);
    }
    g_voltage = constrain(g_voltage,15.0f,30.0f);

    if(millis() - lastTempMs >= 1000UL){
        lastTempMs=millis();
        sensors.requestTemperatures();
        float t = sensors.getTempCByIndex(0);
        if(t != DEVICE_DISCONNECTED_C){
            g_temperature = t;
            Serial.printf("[Temperature] %.2f°C\n", g_temperature);
        }
    }

    if(millis()-lastOpcMs>=1000UL){
        lastOpcMs=millis();
        sendOpcData(g_voltage,g_temperature);
    }

}