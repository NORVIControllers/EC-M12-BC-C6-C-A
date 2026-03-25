#include <STM32LowPower.h>
#include <ModbusMaster.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <SPI.h>
#include "SD.h"
#include <ArduinoJson.h>

#define TINY_GSM_MODEM_SIM7070
#define SerialMon Serial

#define RS485_RX PB7
#define RS485_TX PB6
#define FC PB5

#define SCL_PIN PA9
#define SDA_PIN PA10

#define GSM_POWER PA1
#define GSM_TX PA2
#define GSM_RX PA3
#define DTR_PIN PB12

#define MISO_PIN PA6
#define MOSI_PIN PA7
#define SCLK_PIN PA5

#define BOOST_EN PA4

#define SD_chipSelect PB0
Sd2Card card;
SdVolume volume;
SdFile root;

HardwareSerial Serial1(RS485_RX, RS485_TX);
HardwareSerial Serial2(GSM_RX, GSM_TX);
TwoWire Wire2(SDA_PIN, SCL_PIN);

#include <TinyGsmClient.h>
#include <PubSubClient.h>

#define GSM_AUTOBAUD_MIN 4800
#define GSM_AUTOBAUD_MAX 9600
#define TINY_GSM_USE_GPRS true
#define GSM_PIN ""

const char* topic = "Values";
const char apn[] = "dialogbb";
const char gprsUser[] = "";
const char gprsPass[] = "";
const char* mqttServer = "mqtt.thingsboard.cloud";
const int mqttPort = 1883;
const char* accessToken = "AHkTa9wsy4eor4yBiHun";

TinyGsm modem(Serial2);
TinyGsmClient client(modem);
PubSubClient mqtt(client);

uint32_t lastReconnectAttempt = 0;
unsigned long lastPublishTime = 0;
unsigned long int mqtt_interval = 30000;

const uint32_t DEEP_SLEEP_MS = 180000; // 60s example

Adafruit_ADS1115 ads1;
#define VOLTAGE_DIVIDER_RATIO 0.5
const float V_Factor = 3.3 / 1.65;
ModbusMaster node;

// ------------------ Modbus Callbacks ------------------
void preTransmission() {
  digitalWrite(FC, HIGH);
}
void postTransmission() {
  digitalWrite(FC, LOW);
}

// ------------------ Helper Functions ------------------
bool waitForModemOK(uint32_t timeout_ms) {
  uint32_t start = millis();
  String resp;
  while (millis() - start < timeout_ms) {
    while (Serial2.available()) resp += (char)Serial2.read();
    if (resp.indexOf("OK") != -1) return true;
    delay(10);
  }
  return false;
}

void safeSerial2End() {
  Serial2.flush();
  Serial2.end();
}
void safeSerial1End() {
  Serial1.flush();
  Serial1.end();
}
void safeSerialEnd() {
  Serial.flush();
  Serial.end();
}


void safeSerial2Begin() {
  Serial2.begin(9600);
  delay(50);
  TinyGsmAutoBaud(Serial2, GSM_AUTOBAUD_MIN, GSM_AUTOBAUD_MAX);
  delay(1000);
}

void modemForceLTE() {
  SerialMon.println("Forcing LTE only mode...");
  Serial2.println("AT+CNMP=38");  // LTE only
  delay(200);
  Serial2.println("AT+CMNB=1");   // Cat-M1 only
  delay(200);
}

bool mqttConnect() {
  SerialMon.print("Connecting to ThingsBoard MQTT... ");
  String clientId = "STM32Client-" + String(random(0xFFFF), HEX);
  if (mqtt.connect(clientId.c_str(), accessToken, "")) {
    SerialMon.println("connected!");
    return true;
  } else {
    SerialMon.print("failed, rc=");
    SerialMon.println(mqtt.state());
    return false;
  }
}

// Check if modem entered DTR sleep
bool isModemSleeping() {
  Serial2.println("AT+CSCLK?");
  delay(500);
  String resp = "";
  while (Serial2.available()) resp += (char)Serial2.read();
  SerialMon.print("CSCLK response: "); SerialMon.println(resp);
  return resp.indexOf("+CSCLK: 1") != -1;
}

// ------------------ Setup ------------------
void setup() {
  SerialMon.begin(9600);
  delay(200);

  pinMode(GSM_POWER, OUTPUT);
  pinMode(DTR_PIN, OUTPUT);
  pinMode(BOOST_EN, OUTPUT);
  pinMode(FC, OUTPUT);

  LowPower.begin();

  digitalWrite(DTR_PIN, LOW);
  // Reset GSM
  digitalWrite(GSM_POWER, HIGH);
  delay(200);
  digitalWrite(GSM_POWER, LOW);
  delay(2000);

  Serial1.begin(9600);
  node.begin(1, Serial1);
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);

  safeSerial2Begin();

  SPI.begin();

  SerialMon.println("Initializing modem...");
  modem.restart();
  String modemInfo = modem.getModemInfo();
  SerialMon.print("Modem Info: "); SerialMon.println(modemInfo);

#if TINY_GSM_USE_GPRS
  if (GSM_PIN && modem.getSimStatus() != 3) modem.simUnlock(GSM_PIN);
#endif

   modem.sendAT("+CNMP=38");   // LTE only
   waitForModemOK(1000);

  SerialMon.print("Waiting for network...");
  if (!modem.waitForNetwork()) {
    SerialMon.println(" fail");
    delay(10000);
    return;
  }
  SerialMon.println(" success");

#if TINY_GSM_USE_GPRS
  SerialMon.print(F("Connecting to APN "));
  SerialMon.print(apn);
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    SerialMon.println(" fail");
    delay(10000);
    return;
  }
  SerialMon.println(" success");
#endif

  mqtt.setServer(mqttServer, mqttPort);
  mqtt.setKeepAlive(mqtt_interval / 1000);
  mqtt.setSocketTimeout(mqtt_interval / 1000);

  Wire2.begin();
  delay(1000);
  if (!ads1.begin(0x49)) {
    SerialMon.println("Failed to initialize ADS1115.");
    while (1);
  }
  ads1.setGain(GAIN_ONE);

  SerialMon.println("Setup complete.");
}

// ------------------ Loop ------------------
void loop() {
  SerialMon.println("System awake!");

  // Enable peripherals
  digitalWrite(BOOST_EN, HIGH);
  delay(5000);

  // Ensure network connected
  if (!modem.isNetworkConnected()) {
    SerialMon.println("Network disconnected, reconnecting...");
    if (!modem.waitForNetwork(180000L, true)) {
      SerialMon.println("Network reconnect failed!");
      delay(10000);
      return;
    }
  }

#if TINY_GSM_USE_GPRS
  if (!modem.isGprsConnected()) {
    SerialMon.print("Connecting to APN: "); SerialMon.println(apn);
    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
      SerialMon.println("GPRS connect fail");
      delay(10000);
      return;
    }
  }
#endif

  // MQTT connect
  if (!mqtt.connected()) mqttConnect();
  mqtt.loop();

  // ------------------ Read sensors ------------------
  float processed_cm = NAN;
  float realtime_cm = NAN;

  uint8_t res1 = node.readHoldingRegisters(0x0100, 1);
  if (res1 == node.ku8MBSuccess) processed_cm = node.getResponseBuffer(0) / 10.0;
  else SerialMon.print("Modbus error (processed): "); SerialMon.println(res1, HEX);
  delay(1000);
  uint8_t res2 = node.readHoldingRegisters(0x0101, 1);
  if (res2 == node.ku8MBSuccess) realtime_cm = node.getResponseBuffer(0) / 10.0;
  else SerialMon.print("Modbus error (realtime): "); SerialMon.println(res2, HEX);

  float voltage = (ads1.readADC_SingleEnded(2) * 4.096 / 32767.0) * V_Factor;
  SerialMon.print("Battery Voltage: "); SerialMon.println(voltage);

  // Publish to MQTT
  StaticJsonDocument<200> doc;
  doc["Processed_cm"] = processed_cm;
  doc["Realtime_cm"] = realtime_cm;
  doc["Battery"] = voltage;
  String jsonStr;
  serializeJson(doc, jsonStr);
  mqtt.publish("v1/devices/me/telemetry", jsonStr.c_str());
  SerialMon.print("Published: "); SerialMon.println(jsonStr);

  delay(1000);

  // ------------------ Prepare for sleep ------------------
  SerialMon.println("Preparing modem & MCU for sleep...");

  // Disconnect MQTT and GPRS
  if (mqtt.connected()) mqtt.disconnect();
#if TINY_GSM_USE_GPRS
  if (modem.isGprsConnected()) modem.gprsDisconnect();
#endif


  // Keep DTR LOW for 2s to ensure modem is awake
  digitalWrite(DTR_PIN, LOW);
  delay(2000);

  // Enable DTR sleep
  modem.sendAT("+CSCLK=1");
  delay(200);
  digitalWrite(DTR_PIN, HIGH); // allow modem to sleep
  delay(3000);

   safeSerial2End();
   SPI.end();
   Wire2.end();
  safeSerial1End();
  digitalWrite(BOOST_EN, LOW);
  delay(500);

  safeSerialEnd();
  LowPower.sleep(180000);
   // SerialMon.println("STM32 WAKE UP...");
 
SerialMon.begin(9600);
if (!wakeModemFromDTRSleep()) {
  SerialMon.println("Forcing modem restart...");
  digitalWrite(GSM_POWER, HIGH);
  delay(1200);
  digitalWrite(GSM_POWER, LOW);
  delay(5000);
  safeSerial2Begin();
}

#if TINY_GSM_USE_GPRS
if (!modem.isGprsConnected()) {
    SerialMon.println("Reconnecting APN...");
    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
        SerialMon.println("GPRS reconnect failed!");
    }
}
#endif

  setupPeripherals();
  SerialMon.println("Wake sequence complete, ready for next cycle.\n");
}


bool wakeModemFromDTRSleep() {
  SerialMon.println("Waking modem from DTR sleep...");

  // 2. Start UART

  // 1. Allow wake
  digitalWrite(DTR_PIN, LOW);

  safeSerial2Begin();

  // 3. Disable sleep mode explicitly
  Serial2.println("AT+CSCLK=0");
   waitForModemOK(1000);

  Serial2.println("AT+CFUN=1");
  waitForModemOK(1000);

  // 4. Confirm modem is alive
  for (int i = 0; i < 10; i++) {
    Serial2.println("AT");
    if (waitForModemOK(1000)) {
      SerialMon.println("Modem awake and responsive.");
      return true;
    }
    delay(300);
  }

  SerialMon.println("Modem did not wake!");
  return false;
}

void setupPeripherals() {

  Serial1.begin(9600);
  SPI.begin();
  Wire2.begin();
  if (!ads1.begin(0x49)) {
    SerialMon.println("ADS1115 init failed!");
    while (1);
  }
  ads1.setGain(GAIN_ONE);

  // Modbus
  node.begin(1, Serial1);
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);
}
