#include <STM32LowPower.h>
#include <STM32RTC.h>
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_ADS1X15.h>
#include <ArduinoJson.h>
#include <string.h>
#include <stdlib.h>

// -----------------------------
// SIM7070 / TinyGSM
// -----------------------------
#define TINY_GSM_MODEM_SIM7070
#define SerialMon Serial
#define GSM_AUTOBAUD_MIN 4800
#define GSM_AUTOBAUD_MAX 9600

#include <TinyGsmClient.h>

TinyGsm modem(Serial1);

// -----------------------------
// SIM card credentials
// -----------------------------
#define GSM_PIN ""
const char apn[] = "";  // SIM APN
const char gprsUser[] = "";
const char gprsPass[] = "";

// -----------------------------
// Azure IoT Hub X.509 configuration
// -----------------------------
const char* IOT_HUB_HOST = "";
const char* DEVICE_ID = "";
const int AZURE_MQTT_PORT = 8883;

// -----------------------------
// Certificates (PEM format)
// -----------------------------//ROOT CA IS SAME FOR AZURE IOT HUB
String  rootCAPem =
"-----BEGIN CERTIFICATE-----\n" 
"MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh\n"
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"
"d3cuZGlnaWNlcnQuNVGBNJBVNBVMBNMN,JHJJGHFQ2VydCBHbG9iYWwgUm9vdCBH\n"
"MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT\n"
"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n"
"b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG\n"
"9w0BAQEFAAOCAQ8MBNHMBNVMBVNMBNX/RBMBVMBVMBNMBVNMBrohCgiN9RlUyfuI\n"
"2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx\n"
"1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ\n"
"q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz\n"
"tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ\n"
"vIOlCsRnKNMBVNMBMBNMBNMNVMBVMHNMBNMBNNbq7nMWxM4MphQIDAQABo0IwQDAP\n"
"BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV\n"
"5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY\n"
"1Yl9PMWLSn/pBNMBVMBVMHYKJHJL,.JKLKJj8kVnNeyIv/iPsGEMNKSuIEyExtv4\n"
"NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG\n"
"Fdtom/BVMVBNMBVMBMBNVMBNMBNME6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91\n"
"8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe\n"
"pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl\n"
"MrY=\n"
"-----END CERTIFICATE-----\n" ;

//CHANGE THE DEVICE CERT ACCORDING TO YOUR DEVICE CERTIFICATE
String deviceCertPem =
"-----BEGIN CERTIFICATE-----\n" 
"MIIBqDCCAU6gAwIBAgIUE50ji4nvmuhqFAob4bJPOzv8Rz0wCgYIKoZIzj0EAwIw\n" 
"MDELMAkGA1UEBhMCTEsxDjAMBgNVBAoMBU5PUlZJMREwDwYDVQQDDAhNeVJvb3RD\n" 
"QTAeFw0yHGJHGFKJHKLM.,LYUYTGKJMHGMKJKHMHKjRaMDQxCzAJBgNVBAYTAkxL\n" 
"MQ4wDAYDVQQKDAVOT1JWSTEVMBMGA1UEAwwMRUMtTTEyLUJDLUM2MFkwEwYHKoZI\n" 
"zj0CAQYIKoZIzj0DAQcDQgAEgTZOMVuK0RxQWHoscn9vdRCkO+NTDan3bc5c1fTv\n" 
"Fd58ISG7JGHJYUIYUOKJ.KJM.;OL'POKJLHJKM,HJKHJMx6NCMEAwHQYDVR0OBBYE\n" 
"FCFVMHge3c55mwVpEmNnqrS8xQ9SMB8GA1UdIwQYMBaAFLc/gIFsiaaAU0s/ZwF+\n" 
"Nn7mIoHHGJHGKJHLOKIODHFGHFGJKGKJTGYre117hNVw6wKFmigFvK70RpJt9qyn\n" 
"Rzlif+jHwGh0AiEA8J0e5pZBJF+LKpCJKRimbeWOO8f6yotCPeXxwjzQGF0=\n" 
"-----END CERTIFICATE-----\n" ;

String  deviceKeyPem =
"-----BEGIN EC PRIVATE KEY-----\n"
"MHcCAQEEIOEBVMHGKJHKH;POBFGFDHGHMMMMMFJNJGHJNNzq/ukloAoGCCqGSM49\n"
"AwEHJGJGHKMHJLOIPRTYTR,J,JHLLIGDHFGFO+NTDan3bc5c1fTvFd58ISG7uLQx\n"
"fWwLTdTuYKpsSQARyS3fcHo8utfcTQuMxw==\n"
"-----END EC PRIVATE KEY-----\n";

// -----------------------------
// Pins
// -----------------------------
#define SCL_PIN PA9
#define SDA_PIN PA10

#define GSM_POWER PA1
#define GSM_TX PA2
#define GSM_RX PA3

#define BOOST_EN PA4

#define TANK_HEIGHT_MM 1000

TwoWire Wire2(SDA_PIN, SCL_PIN);
HardwareSerial Serial1(GSM_RX, GSM_TX); // GSM_RX, GSM_TX

STM32RTC &rtc = STM32RTC::getInstance();
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};


Adafruit_ADS1115 ads1;
#define VOLTAGE_DIVIDER_RATIO 0.5
const float mA_Factor = 4.3115789 / 3269.826;

uint8_t monthFromString(const char *mon) {
  if (strncmp(mon, "Jan", 3) == 0) return 1;
  if (strncmp(mon, "Feb", 3) == 0) return 2;
  if (strncmp(mon, "Mar", 3) == 0) return 3;
  if (strncmp(mon, "Apr", 3) == 0) return 4;
  if (strncmp(mon, "May", 3) == 0) return 5;
  if (strncmp(mon, "Jun", 3) == 0) return 6;
  if (strncmp(mon, "Jul", 3) == 0) return 7;
  if (strncmp(mon, "Aug", 3) == 0) return 8;
  if (strncmp(mon, "Sep", 3) == 0) return 9;
  if (strncmp(mon, "Oct", 3) == 0) return 10;
  if (strncmp(mon, "Nov", 3) == 0) return 11;
  if (strncmp(mon, "Dec", 3) == 0) return 12;
  return 1;
}

uint8_t dayOfWeekFromDate(uint16_t year, uint8_t month, uint8_t day) {
  if (month < 3) {
    month += 12;
    year--;
  }
  uint16_t k = year % 100;
  uint16_t j = year / 100;
  uint8_t h = (day + ((13 * (month + 1)) / 5) + k + (k / 4) + (j / 4) + (5 * j)) % 7;
  return (h + 6) % 7;
}

void parseBuildDateTime(uint16_t &year, uint8_t &month, uint8_t &day, uint8_t &hour, uint8_t &minute, uint8_t &second) {
  month = monthFromString(__DATE__);
  day = (uint8_t)atoi(__DATE__ + 4);
  year = (uint16_t)atoi(__DATE__ + 7);
  hour = (uint8_t)atoi(__TIME__);
  minute = (uint8_t)atoi(__TIME__ + 3);
  second = (uint8_t)atoi(__TIME__ + 6);
}

// -----------------------------
// Helpers
// -----------------------------
void waitForOK(unsigned long timeout=5000) {
  unsigned long start = millis();
  while (millis() - start < timeout) {
    if (Serial1.find("OK")) return;
  }
  SerialMon.println("Timeout waiting for OK");
}

bool Modem_Init_WithRetry(uint8_t maxRetries = 3) {
  for (uint8_t attempt = 1; attempt <= maxRetries; attempt++) {
    Serial.print("Modem init attempt: ");
    Serial.println(attempt);

    Modem_Init();

    // Check if modem responded properly
    Serial1.println("AT");
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
        Serial1.println("AT+CPOWD=1");

        if (waitForModemResponse("NORMAL POWER DOWN", NULL, 7000)) {
            // Small delay to allow URCs to finish
            delay(500);

            // Verify modem is really OFF
            Serial1.println("AT");
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
        while (Serial1.available()) Serial1.read();

        // Verify modem is OFF
        Serial1.println("AT");
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

void setup() {
  SerialMon.begin(9600);
  delay(100);
  pinMode(BOOST_EN, OUTPUT);

  Serial.println("Start Pressure sensor operation");
  LowPower.begin();                   //Initiate low power mode

  SPI.begin();
  delay(1000);

  if (!Modem_Init_WithRetry()) {
   Serial.println("Going to shutdown due to modem init failure...");
   LowPower.shutdown(10000);  // sleep 10 sec, then retry
  }

  if(!sim7070_upload_ca()) {
    Serial.println("Failed to upload CA file");
    while(1);
  }
   sim7070_init_tls();
  if (!azure_mqtt_configure()) {
    Serial.println("MQTT config failed");
    return;
  }
    if (!azure_mqtt_connect()) return;

   Wire2.begin();
   delay(1000);
   I2C_SCAN();
   delay(1000);

  if (!ads1.begin(0x49)) {
    Serial.println("Failed to initialize ADS 1 .");
    while (1);
  }
  ads1.setGain(GAIN_ONE);  // 1x gain +/- 4.096V  (1 bit = 0.125mV)

   RTC_Check();
   delay(1000);
  
}

void loop() {
  
  Serial.println("System is awake!");
  digitalWrite(BOOST_EN, HIGH);     // Enable RS485 / INA196 / Booster
  delay(5000);

  int16_t adc0, adc1, adc2, adc3;

  adc0 = ads1.readADC_SingleEnded(0) * mA_Factor;
  adc1 = ads1.readADC_SingleEnded(1) * mA_Factor;
  adc2 = ads1.readADC_SingleEnded(2);

  float voltage2 = adc2 * 0.125 / 1000.0 / VOLTAGE_DIVIDER_RATIO;

  Serial.print("AIN1: "); Serial.print(adc0); Serial.println("  ");
  Serial.print("AIN2: "); Serial.print(adc1); Serial.println("  ");
  Serial.print("Voltage: "); Serial.print(voltage2); Serial.println("  ");

  float fuel_percent = (adc1 - 4.0) * 100.0 / 16.0;
  fuel_percent = constrain(fuel_percent, 0, 100);

  // Fuel height
  float fuel_level_mm = (fuel_percent / 100.0) * 1000;

  // Fault detection
  bool fuel_sensor_fault = false;
  if (adc1 < 3.6 || adc1 > 22.0) {
    fuel_sensor_fault = true;
  }

  StaticJsonDocument<200> doc;
  doc["fuel_level_percent"] = fuel_percent;
  doc["fuel_level_mm"]      = fuel_level_mm;
  doc["battery_voltage"]    = voltage2;
  doc["fuel_sensor_fault"]  = fuel_sensor_fault;

  String jsonStr;
  serializeJson(doc, jsonStr);

  SerialMon.print("Publishing: "); SerialMon.println(jsonStr);
  if (azure_publish_payload(jsonStr.c_str())) {
        Serial.println("[MQTT] Data sent successfully");
    } else {
        Serial.println("[MQTT] Send failed");
    }

  delay(2000);
  Serial.println("Entering low power mode...");

  SPI.end();
  delay(1000);
  Wire2.end();
  delay(1000);

  digitalWrite(BOOST_EN, LOW);     // Disable RS485 / INA196 / Booster
  delay(1000);
  if (!PowerOffModem_WithVerify()) {
     Serial.println("Warning: Modem still ON before shutdown");
  };

  LowPower.shutdown(3000);  

}


// -----------------------------
// Modem initialization
// -----------------------------
void Modem_Init() {
  Serial1.begin(9600);
  pinMode(GSM_POWER, OUTPUT);
  digitalWrite(GSM_POWER, HIGH);
  delay(200);
  digitalWrite(GSM_POWER, LOW);
  delay(2000);

  TinyGsmAutoBaud(Serial1, GSM_AUTOBAUD_MIN, GSM_AUTOBAUD_MAX);
  delay(6000);
  
  modem.sendAT("+CNMP=13"); modem.waitResponse();
  modem.restart();
  
  String modemInfo = modem.getModemInfo();
  Serial.print("Modem Info: "); Serial.println(modemInfo);

  
#if TINY_GSM_USE_GPRS
  if (GSM_PIN && modem.getSimStatus()!=3) modem.simUnlock(GSM_PIN);
#endif
  if (!modem.waitForNetwork()) { Serial.println("Network fail"); return; }
#if TINY_GSM_USE_GPRS
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) { Serial.println("GPRS fail"); return; }
#endif
  Serial.println("Network connected");
}

bool waitForModemResponse(const char* successToken,
                          const char* errorToken,
                          uint32_t timeoutMs)
{
  unsigned long start = millis();

  while (millis() - start < timeoutMs) {
    if (Serial1.available()) {
      String resp = Serial1.readString();
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

bool sim7070_upload_ca() {

  Serial.println("[SIM7070] Uploading CA certificate...");

  int ca_len = rootCAPem.length();
  Serial.print("CA length: ");
  Serial.println(ca_len);

  // 1. Activate PDP
  Serial1.println("AT+CNACT=0,1");
  delay(3000);
  while (Serial1.available()) Serial1.read();

  // 2. Init filesystem
  Serial1.println("AT+CFSINIT");
  delay(1000);
  while (Serial1.available()) Serial1.read();

  // 3. Delete old CA (optional)
  Serial1.println("AT+CCERTDELE=\"ca.crt\"");
  delay(500);
  while (Serial1.available()) Serial1.read();

  // 4. Start upload
  String cmd = "AT+CFSWFILE=3,\"ca.crt\",0," + String(ca_len) + ",10000";
  Serial.print("Send -> ");
  Serial.println(cmd);
  Serial1.println(cmd);

  // 5. Wait for DOWNLOAD
  if (!waitForModemResponse("DOWNLOAD", "ERROR", 5000)) {
    Serial.println("CFSWFILE did not return DOWNLOAD, check AT+CFSINIT and PDP!");
    return false;
  }

  // 6. Send certificate content
  Serial1.print(rootCAPem);
  delay(500);

  // 7. Finish with Ctrl+Z
  Serial1.write(26);

  // 8. Wait for OK
  if (!waitForModemResponse("OK", "ERROR", 5000)) {
    Serial.println("CA upload failed or timed out!");
    return false;
  }

  int dev_cert_len = deviceCertPem.length();
  Serial.print("Device Cert length: ");
  Serial.println(dev_cert_len);

  Serial1.println("AT+CCERTDELE=\"myclient.crt\"");
  delay(500);
  while (Serial1.available()) Serial1.read();

  String cmd1 = "AT+CFSWFILE=3,\"myclient.crt\",0," + String(dev_cert_len) + ",10000";
  Serial.print("Send -> ");
  Serial.println(cmd1);
  Serial1.println(cmd1);

  // 5. Wait for DOWNLOAD
  if (!waitForModemResponse("DOWNLOAD", "ERROR", 5000)) {
    Serial.println("CFSWFILE did not return DOWNLOAD, check AT+CFSINIT and PDP!");
    return false;
  }

  // 6. Send certificate content
  Serial1.print(deviceCertPem);
  delay(500);

  // 7. Finish with Ctrl+Z
  Serial1.write(26);
  
  // 8. Wait for OK
  if (!waitForModemResponse("OK", "ERROR", 5000)) {
    Serial.println("CA upload failed or timed out!");
    return false;
  }

  int dev_key_len = deviceKeyPem.length();
  Serial.print("Device Key length: ");
  Serial.println(dev_key_len);

  Serial1.println("AT+CCERTDELE=\"myclient.key\"");
  delay(500);
  while (Serial1.available()) Serial1.read();

  String cmd2 = "AT+CFSWFILE=3,\"myclient.key\",0," + String(dev_key_len) + ",10000";
  Serial.print("Send -> ");
  Serial.println(cmd2);
  Serial1.println(cmd2);

  // 5. Wait for DOWNLOAD
  if (!waitForModemResponse("DOWNLOAD", "ERROR", 5000)) {
    Serial.println("CFSWFILE did not return DOWNLOAD, check AT+CFSINIT and PDP!");
    return false;
  }

  // 6. Send certificate content
  Serial1.print(deviceKeyPem);
  delay(500);

  // 7. Finish with Ctrl+Z
  Serial1.write(26);
  
  // 8. Wait for OK
  if (!waitForModemResponse("OK", "ERROR", 5000)) {
    Serial.println("CA upload failed or timed out!");
    return false;
  }

  // 9. Close filesystem
  Serial1.println("AT+CFSTERM");
  if (!waitForModemResponse("OK", "ERROR", 5000)) {
    Serial.println("CFSTERM failed or timed out!");
    return false;
  }

  // 10. Verify
  Serial1.println("AT+CCERTLIST");
  delay(1000);
  while (Serial1.available()) {
    Serial.write(Serial1.read());
  }

  Serial.println("[SIM7070] CA certificate uploaded ");
  return true;
}

bool sim7070_init_tls() {
  Serial.println("[SIM7070] Init TLS...");

  // Reset SSL profile
  modem.sendAT("+SMSSL=0");
  modem.waitResponse(2000);

  modem.sendAT("+CSSLCFG=\"sslversion\",0,3");   // TLS 1.2
  if (modem.waitResponse() != 1) return false;

  modem.sendAT("+CSSLCFG=\"authmode\",0,2");     // server auth
  if (modem.waitResponse() != 1) return false;

  modem.sendAT("+CSSLCFG=\"ignorelocaltime\",0,1");
  if (modem.waitResponse() != 1) return false;

  Serial.println("[SIM7070] TLS ready");
  return true;
}

bool azure_mqtt_configure() {
  
  Serial.println("[AZURE] Configuring MQTT...");
  
  String username = String(IOT_HUB_HOST) + "/" + DEVICE_ID + "/?api-version=2021-04-12";
  char buf[200];
  
  // 1. URL
  snprintf(buf, sizeof(buf), "+SMCONF=\"URL\",\"%s\",%d", IOT_HUB_HOST, AZURE_MQTT_PORT);
  Serial.print("AT -> "); Serial.println(buf);
  modem.sendAT(buf);
  int resp = modem.waitResponse(5000);
  Serial.print("Response: "); Serial.println(resp);
  if (resp != 1) return false;

    //  Username
  snprintf(buf, sizeof(buf), "+SMCONF=\"USERNAME\",\"%s\"", username.c_str());
  Serial.print("AT -> "); Serial.println(buf);
  modem.sendAT(buf);
  resp = modem.waitResponse(5000);
  Serial.print("Response: "); Serial.println(resp);
  if (resp != 1) return false;

   // 2. Keepalive
  Serial.println("AT -> +SMCONF=\"KEEPTIME\",60");
  modem.sendAT("+SMCONF=\"KEEPTIME\",60");
  resp = modem.waitResponse(5000);
  Serial.print("Response: "); Serial.println(resp);
  if (resp != 1) return false;

  //3. Clean session
  Serial.println("AT -> +SMCONF=\"CLEANSS\",1");
  modem.sendAT("+SMCONF=\"CLEANSS\",1");
  resp = modem.waitResponse(5000);
  Serial.print("Response: "); Serial.println(resp);
  if (resp != 1) return false;
  
  delay(1500);
 
  // 2. Client ID
  snprintf(buf, sizeof(buf), "+SMCONF=\"CLIENTID\",\"%s\"", DEVICE_ID);
  Serial.print("AT -> "); Serial.println(buf);
  modem.sendAT(buf);
  resp = modem.waitResponse(5000);
  Serial.print("Response: "); Serial.println(resp);
  if (resp != 1) return false;
  
  // rootCA.pem is CA certificate
  Serial.println("AT -> +CSSLCFG=\"CONVERT\",2,\"ca.crt\"");
  modem.sendAT("+CSSLCFG=\"CONVERT\",2,\"ca.crt\"");
  resp = modem.waitResponse(5000);
  Serial.print("Response: "); Serial.println(resp);
  if (resp != 1) return false;

    // myclient.crt is client certificate
  Serial.println("AT -> +CSSLCFG=\"CONVERT\",1,\"myclient.crt\",\"myclient.key\"");
  modem.sendAT("+CSSLCFG=\"CONVERT\",1,\"myclient.crt\",\"myclient.key\"");
  resp = modem.waitResponse(5000);
  Serial.print("Response: "); Serial.println(resp);
  if (resp != 1) return false;

  // Enable SSL with CA file (no client cert for SAS)
  modem.sendAT("+SMSSL=1,\"ca.crt\",\"myclient.crt\"");
  if (modem.waitResponse(10000) != 1) {
    Serial.println("TLS enable failed");
    return false;
  }

  Serial.println("[AZURE] MQTT configured");
  return true;

}

bool azure_mqtt_connect() {
  Serial.println("[AZURE] Connecting MQTT...");

  modem.sendAT("+SMCONN");
  int res = modem.waitResponse(20000); // 20s for TLS handshake
  if (res != 1) {
    Serial.println("MQTT connect failed");
    return false;
  }

  Serial.println("MQTT connected");
  return true;
}

bool azure_publish_payload(const String& payload) {

  String topic = String("devices/") + DEVICE_ID + "/messages/events/";

  modem.sendAT("+SMPUB=\"" + topic + "\"," + payload.length() + ",1,1");
  if (modem.waitResponse(">") != 1) return false;

  modem.stream.print(payload);
  if (modem.waitResponse(10000) != 1) return false;

  Serial.println("[AZURE] Telemetry sent");
  return true;
}

void displayTime(void) {
  uint8_t day = rtc.getDay();
  uint8_t month = rtc.getMonth();
  uint16_t year = 2000 + rtc.getYear();
  uint8_t weekDay = rtc.getWeekDay();

  if (weekDay > 6) {
    weekDay = dayOfWeekFromDate(year, month, day);
  }

  Serial.print(year, DEC);
  Serial.print('/');
  Serial.print(month, DEC);
  Serial.print('/');
  Serial.print(day, DEC);
  Serial.print(" ");
  Serial.print(daysOfTheWeek[weekDay]);

  Serial.print(rtc.getHours(), DEC);
  Serial.print(':');
  Serial.print(rtc.getMinutes(), DEC);
  Serial.print(':');
  Serial.print(rtc.getSeconds(), DEC);
  Serial.println();
  delay(1000);

}

void RTC_Check(){
  rtc.setClockSource(STM32RTC::LSE_CLOCK);
  rtc.begin();
  
  bool rtcLooksUninitialized =
      (rtc.getMonth() == 1) &&
      (rtc.getDay() == 1) &&
      (rtc.getYear() <= 1) &&
      (rtc.getHours() == 0) &&
      (rtc.getMinutes() == 0);

  if (rtcLooksUninitialized) {
    rtc.setDate(3, 25, 3, 26);  
    rtc.setTime(10, 56, 0);     
    Serial.println("Internal RTC was not set, initialized to fixed startup date/time.");
  }

  int a = 1;
  while (a < 6) {
    displayTime();   // printing time function for serial
    a = a + 1;
  }
}
