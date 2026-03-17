
/*
 * EC-M12-BC-C6-B-2.1 Low Power
 */

#include <STM32LowPower.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include "RTClib.h"
#include <SPI.h>
#include "SD.h"

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
//#define SD_PWR PB0

// SD Paramerters
#define SD_chipSelect PB0
Sd2Card card;
SdVolume volume;
SdFile root;

TwoWire Wire2(SDA_PIN, SCL_PIN);

//                      RX    TX
HardwareSerial Serial1(RS485_RX, RS485_TX);
HardwareSerial Serial2(GSM_RX, GSM_TX);

RTC_DS3231 rtc; 
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

Adafruit_ADS1115 ads1;
const float V_Factor = 3.3 / 1.65;
const float mA_Factor = 4.096 / 3269.826;

uint32_t ShutdownPeriod = 30000;     //Shutdown period in milliseconds


void setup() {
  Serial.begin(9600);
  delay(500);
  pinMode(FC, OUTPUT); 
  pinMode(GSM_POWER, OUTPUT); 
  pinMode(BOOST_EN, OUTPUT); 
  pinMode(SD_PWR, OUTPUT);
  Serial.println("EC-M12-BC-C6-B-2.1 Low Power");
  
  digitalWrite(SD_PWR, HIGH);
 
  digitalWrite(FC, HIGH);
  delay(1000);
  digitalWrite(BOOST_EN, LOW);     // Disable RS485 / INA196 / Booster
  delay(1000);
//  digitalWrite(GSM_POWER, HIGH);   // Turn off GSM
//  delay(2000);

  LowPower.begin();                   //Initiate low power mode

  Serial1.begin(9600);
  delay(100); 
  Serial2.begin(9600); 
  delay(100);
  
  SPI.begin();
  delay(2000);

  digitalWrite(GSM_POWER, HIGH);   // Set GSM_RESET pin LOW to reset
  delay(200);                     // Hold reset for 200ms
  digitalWrite(GSM_POWER, LOW);  // Release reset
  delay(2000);

  Wire2.begin();
  delay(1000);

  if (!ads1.begin(0x49)) {
    Serial.println("Failed to initialize ADS 1 .");
    while (1);
  }
  ads1.setGain(GAIN_ONE);  // 1x gain +/- 4.096V  (1 bit = 0.125mV)

  digitalWrite(BOOST_EN, HIGH);     // Enable RS485 / INA196 / Booster / SD Card
  delay(500);
  
  I2C_SCAN();
  delay(1000);

  RTC_Check();
  delay(1000);

  SD_CHECK();
  delay(1000);

}

void loop() {
  
  digitalWrite(SD_PWR, HIGH);
  digitalWrite(FC, HIGH);                  // Make FLOW CONTROL pin HIGH
  Serial1.println("RS485 01 SUCCESS");     // Send RS485 SUCCESS serially
  delay(100);                              // Wait for transmission of data
  digitalWrite(FC, LOW);                   // Receiving mode ON                                                 
  delay(100);
  while (Serial1.available()) {  // Check if data is available
    char c = Serial1.read();     // Read data from RS485
    Serial.write(c);             // Print data on serial monitor
  }
  delay(100);

  while (Serial.available()) {
    int inByte = Serial.read();
    Serial2.write(inByte);
  }

  while (Serial2.available()) {
    int inByte = Serial2.read();
    Serial.write(inByte);
  }

  float current0 = ads1.readADC_SingleEnded(1) * mA_Factor;
  float current1 = ads1.readADC_SingleEnded(0) * mA_Factor;
  float voltage2 = (ads1.readADC_SingleEnded(2) * 4.096 / 32767.0) * V_Factor;
  
  Serial.print("Current 0: "); Serial.print(current0); Serial.println(" mA");
  Serial.print("Current 1: "); Serial.print(current1); Serial.println(" mA");
  Serial.print("BATTERY VOLTAGE : "); Serial.print(voltage2); Serial.println(" V");

  delay(2000);
  Serial.println("Entering low power mode...");

  delay(1000);
  SPI.end();
  delay(1000);
  Wire2.end();
  delay(1000);
 
  digitalWrite(FC, LOW); 
  delay(1000);
  digitalWrite(SD_PWR, LOW);
  delay(1000);
  digitalWrite(BOOST_EN, LOW);     // Disable RS485 / INA196 / Booster / SD Card
  delay(1000);
  digitalWrite(GSM_POWER, HIGH);   // Turn off GSM
  delay(2000);                     
  LowPower.shutdown(ShutdownPeriod);         //Enters to the shutdown mode for 2 min.

}

void displayTime(void) {
  DateTime now = rtc.now();
     
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" ");
  Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);

  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();
  delay(1000);

}

void RTC_Check(){
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
  }
 else{
 if (rtc.lostPower()) {
  
    Serial.println("RTC lost power, lets set the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    
  }
 
  int a=1;
  while(a<6)
  {
  displayTime();   // printing time function for oled
  a=a+1;
  }
 }
}

void SD_CHECK(){
  Serial.print("\nInitializing SD card...");
   if (!card.init(SPI_HALF_SPEED, SD_chipSelect)) 
    {   Serial.println("CARD NOT FOUND"); 
    }
    else 
    { 
      Serial.println("SD WORKING"); 
    }
    
    Serial.print("\nCard type: ");
    Serial.println(card.type());
    if (!volume.init(card)) {
      Serial.println("Could not find FAT16/FAT32 partition.\nMake sure you've formatted the card");
      return;
    }
    if (!SD.begin(SD_chipSelect)) {
      Serial.println("Card failed, or not present");
      // don't do anything more:
      return;
    } 

}
void I2C_SCAN() {
  byte error, address;
  int deviceCount = 0;

  Serial.println("Scanning...");

  for (address = 1; address < 127; address++) {
    Wire2.beginTransmission(address);
    error = Wire2.endTransmission();

    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.print(address, HEX);
      Serial.println("  !");

      deviceCount++;
      delay(1);  // Wait for a moment to avoid overloading the I2C bus
    }
    else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.println(address, HEX);
    }
  }

  if (deviceCount == 0) {
    Serial.println("No I2C devices found\n");
  }
  else {
    Serial.println("Scanning complete\n");
  }
}
