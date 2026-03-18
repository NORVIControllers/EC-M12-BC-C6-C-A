
#include <STM32LowPower.h>
#include <ModbusMaster.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include "RTClib.h"
#include <SPI.h>
#include "SD.h"
#include <ArduinoJson.h>

#define TINY_GSM_MODEM_SIM7070

#define SerialMon Serial

#define GSM_AUTOBAUD_MIN 4800
#define GSM_AUTOBAUD_MAX 9600

#define TINY_GSM_USE_GPRS true
#define GSM_PIN ""

const char apn[] = "dialogbb";
const char gprsUser[] = "";
const char gprsPass[] = "";

const char* mqttServer = "mqtt.thingsboard.cloud";
const int mqttPort = 1883;
const char* accessToken = "AHkTa9wsy4eor4yBiHun";

#include <TinyGsmClient.h>
#include <PubSubClient.h>

// Just in case someone defined the wrong thing..
#if TINY_GSM_USE_GPRS && not defined TINY_GSM_MODEM_HAS_GPRS
#undef TINY_GSM_USE_GPRS
#undef TINY_GSM_USE_WIFI
#define TINY_GSM_USE_GPRS false
#define TINY_GSM_USE_WIFI true
#endif
#if TINY_GSM_USE_WIFI && not defined TINY_GSM_MODEM_HAS_WIFI
#undef TINY_GSM_USE_GPRS
#undef TINY_GSM_USE_WIFI
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false
#endif

TinyGsm modem(Serial2);
TinyGsmClient client(modem);
PubSubClient mqtt(client);

uint32_t lastReconnectAttempt = 0;

#define PUBLISH_INTERVAL 60000  // Publish interval in milliseconds (1 minute)


unsigned long lastPublishTime = 0;
unsigned long int mqtt_interval = 30000;

// === PINS ===
#define RS485_RX PB7
#define RS485_TX PB6
#define FC PB5

#define SCL_PIN PA9
#define SDA_PIN PA10

#define GSM_POWER PA1
#define GSM_TX PA2
#define GSM_RX PA3


#define MISO_PIN PA6
#define MOSI_PIN PA7
#define SCLK_PIN PA5

#define BOOST_EN PA4
//#define SD_EN PB0

// SD Paramerters
#define SD_chipSelect PB0
Sd2Card card;
SdVolume volume;
SdFile root;

TwoWire Wire2(SDA_PIN, SCL_PIN);
HardwareSerial Serial1(RS485_RX, RS485_TX);
HardwareSerial Serial2(GSM_RX, GSM_TX);

RTC_DS3231 rtc;
Adafruit_ADS1115 ads1;
const float V_Factor = 3.3 / 1.65;

ModbusMaster node;

// RS485 control
void preTransmission() {
  digitalWrite(FC, HIGH);
}
void postTransmission() {
  digitalWrite(FC, LOW);
}

// MQTT connect
bool mqttConnect() {
  SerialMon.print("Connecting to MQTT... ");
  String clientId = "STM32Client-" + String(random(0xFFFF), HEX);
  if (mqtt.connect(clientId.c_str(), accessToken, "")) {
    SerialMon.println("Connected!");
    return true;
  }
  SerialMon.println("Failed.");
  return false;
}

bool Modem_Init_WithRetry(uint8_t maxRetries = 3) {
  for (uint8_t attempt = 1; attempt <= maxRetries; attempt++) {
    Serial.print("Modem init attempt: ");
    Serial.println(attempt);

    Modem_Init();

    // Check if modem responded properly
    Serial2.println("AT");
    if (waitForModemResponse("OK", "ERROR", 3000)) {
      Serial.println("Modem initialized successfully");
      return true;
    }

    Serial.println("Modem init failed, retrying...");
    delay(3000);
  }

  Serial.println("Modem didn't initialize after retries");

  return false;
}

void modemPowerToggle() {
  Serial.println("Toggling modem PWRKEY...");

  digitalWrite(GSM_POWER, LOW);
  delay(1500);                  // 1 second pulse
  digitalWrite(GSM_POWER, HIGH);

  delay(5000);                  // wait for modem to react
}

bool PowerOffModem_WithVerify(uint8_t maxRetries = 3) {
  for (uint8_t attempt = 1; attempt <= maxRetries; attempt++) {
    Serial.print("Modem power OFF attempt: ");
    Serial.println(attempt);

    // Step 1: Graceful software shutdown
    Serial2.println("AT+CPOWD=1");

    if (waitForModemResponse("NORMAL POWER DOWN", NULL, 7000)) {
      // Small delay to allow URCs to finish
      delay(500);

      // Verify modem is really OFF
      Serial2.println("AT");
      if (!waitForModemResponse("OK", NULL, 3000)) {
        Serial.println("Modem is OFF (no response)");
        return true;
      }

      Serial.println("Modem still ON after CPOWD, using fallback PWRKEY");
    } else {
      Serial.println("CPOWD failed, using fallback PWRKEY");
    }

    // Step 2: Hardware PWRKEY toggle as fallback
    modemPowerToggle();

    // Flush UART buffer
    while (Serial2.available()) Serial2.read();

    // Verify modem is OFF
    Serial2.println("AT");
    if (!waitForModemResponse("OK", NULL, 3000)) {
      Serial.println("Modem is OFF after PWRKEY fallback");
      return true;
    }

    Serial.println("Modem still ON, retrying...");
    delay(2000); // small delay before next attempt
  }

  Serial.println("Modem can't be powered OFF after 3 attempts");
  return false;
}

bool waitForModemResponse(const char* successToken,
                          const char* errorToken,
                          uint32_t timeoutMs)
{
  unsigned long start = millis();

  while (millis() - start < timeoutMs) {
    if (Serial2.available()) {
      String resp = Serial2.readString();
      Serial.print(resp);

      if (successToken && resp.indexOf(successToken) >= 0) {
        return true;
      }
      if (errorToken && resp.indexOf(errorToken) >= 0) {
        return false;
      }
    }
  }
  return false; // timeout
}

void setup() {
  Serial.begin(9600);

  pinMode(FC, OUTPUT);
  pinMode(GSM_POWER, OUTPUT);
  pinMode(BOOST_EN, OUTPUT);

  Serial.println("Start Ultrasonic level sensor operation");

  LowPower.begin();

  Serial1.begin(9600);
  delay(100);
  Serial2.begin(9600);
  delay(100);

  // Modbus setup
  node.begin(1, Serial1);
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);

  SPI.begin();
  delay(1000);

 if (!Modem_Init_WithRetry()) {
   Serial.println("Going to shutdown due to modem init failure...");
   LowPower.shutdown(10000);  // sleep 10 sec, then retry
  }

  // MQTT Broker setup

  mqtt.setServer(mqttServer, mqttPort);
  mqtt.setKeepAlive(mqtt_interval / 1000);
  mqtt.setSocketTimeout(mqtt_interval / 1000);

  Wire2.begin();

  if (!ads1.begin(0x49)) {
    Serial.println("Failed to initialize ADS 1 .");
    while (1);
  }
  ads1.setGain(GAIN_ONE);  // 1x gain +/- 4.096V  (1 bit = 0.125mV)

  //SD.begin(SD_chipSelect);
}

void loop() {
  Serial.println("\nSystem Awake");
  digitalWrite(BOOST_EN, HIGH);
  delay(5000);

  // Make sure we're still registered on the network
  if (!modem.isNetworkConnected()) {
    SerialMon.println("Network disconnected");
    if (!modem.waitForNetwork(180000L, true)) {
      SerialMon.println(" fail");
      delay(10000);
      return;
    }
    if (modem.isNetworkConnected()) {
      SerialMon.println("Network re-connected");
    }

#if TINY_GSM_USE_GPRS
    // and make sure GPRS/EPS is still connected
    if (!modem.isGprsConnected()) {
      SerialMon.println("GPRS disconnected!");
      SerialMon.print(F("Connecting to "));
      SerialMon.print(apn);
      if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
        SerialMon.println(" fail");
        delay(10000);
        //return;
      }
      if (modem.isGprsConnected()) {
        SerialMon.println("GPRS reconnected");
      }
    }
#endif
  }

  if (!mqtt.connected()) {
    SerialMon.println("=== MQTT NOT CONNECTED ===");
    // Reconnect every 10 seconds
    uint32_t t = millis();
    if (t - lastReconnectAttempt > 10000L) {
      lastReconnectAttempt = t;
      if (mqttConnect()) {
        lastReconnectAttempt = 0;
      }
    }
    delay(100);

    return;
  }

  mqtt.loop();

  // ===========================================
  //   READ DJLK ULTRASONIC SENSOR (RS485)
  // ===========================================

  float processed_cm = NAN;
  float realtime_cm = NAN;

  // ==== Read processed distance (0x0100) ====
  uint8_t result1 = node.readHoldingRegisters(0x0100, 1);
  if (result1 == node.ku8MBSuccess) {
    uint16_t dist_mm = node.getResponseBuffer(0);
    processed_cm = dist_mm / 10.0;
  } else {
    Serial.print("Modbus Error (Processed): 0x");
    Serial.println(result1, HEX);
  }

  delay(200);

  // ==== Read realtime distance (0x0101) ====
  uint8_t result2 = node.readHoldingRegisters(0x0101, 1);
  if (result2 == node.ku8MBSuccess) {
    uint16_t dist_mm = node.getResponseBuffer(0);
    realtime_cm = dist_mm / 10.0;
  } else {
    Serial.print("Modbus Error (Realtime): 0x");
    Serial.println(result2, HEX);
  }

  float voltage = (ads1.readADC_SingleEnded(2) * 4.096 / 32767.0) * V_Factor;
  Serial.print("BATTERY VOLTAGE : "); Serial.print(voltage); Serial.println(" V");


  // ===========================================
  //   PUBLISH TO MQTT
  // ===========================================

  StaticJsonDocument<200> doc;
  doc["Processed_cm"] = processed_cm;
  doc["Realtime_cm"] = realtime_cm;
  doc["Battery"] = voltage;

  String jsonStr;
  serializeJson(doc, jsonStr);

  mqtt.publish("v1/devices/me/telemetry", jsonStr.c_str());
  Serial.print("Published: ");
  Serial.println(jsonStr);

  delay(2000);

  // ===========================================
  //   LOW POWER MODE
  // ===========================================

  SPI.end();
  delay(1000);
  Wire2.end();
  delay(1000);

  digitalWrite(BOOST_EN, LOW);
  delay(1000);

  if (!PowerOffModem_WithVerify()) {
     Serial.println("Warning: Modem still ON before shutdown");
  };

  LowPower.shutdown(900000);   // Sleep 15 minutes
}

void Modem_Init() {
  // GSM power toggle
  digitalWrite(GSM_POWER, HIGH);
  delay(200);
  digitalWrite(GSM_POWER, LOW);
  delay(2000);

  SerialMon.println("Wait...");

  // Set GSM module baud rate
  TinyGsmAutoBaud(Serial2, GSM_AUTOBAUD_MIN, GSM_AUTOBAUD_MAX);
  delay(6000);

  SerialMon.println("Initializing modem...");
  modem.restart();

  String modemInfo = modem.getModemInfo();
  SerialMon.print("Modem Info: ");
  SerialMon.println(modemInfo);

#if TINY_GSM_USE_GPRS
  // Unlock your SIM card with a PIN if needed
  if (GSM_PIN && modem.getSimStatus() != 3) {
    modem.simUnlock(GSM_PIN);
  }
#endif

  SerialMon.print("Waiting for network...");
  if (!modem.waitForNetwork()) {
    SerialMon.println(" fail");
    delay(10000);
    return;
  }
  SerialMon.println(" success");

  if (modem.isNetworkConnected()) {
    SerialMon.println("Network connected");
  }

#if TINY_GSM_USE_GPRS
  SerialMon.print(F("Connecting to "));
  SerialMon.print(apn);
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    SerialMon.println(" fail");
    delay(10000);
    return;
  }
  SerialMon.println(" success");

  if (modem.isGprsConnected()) {
    SerialMon.println("GPRS connected");
  }
#endif
}
