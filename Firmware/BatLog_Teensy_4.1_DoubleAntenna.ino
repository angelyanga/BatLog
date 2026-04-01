#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include "RTClib.h"
#include <LiquidCrystal_I2C.h>

// ========== LCD SETUP ==========
#define LCD_I2C_ADDRESS 0x27
TwoWire myWire2 = Wire2;  // Use pins 24 (SDA2), 25 (SCL2)
LiquidCrystal_I2C lcd(LCD_I2C_ADDRESS, 20, 4);  // LCD at 0x27, 20x4
bool lcdPresent = false;
// ================================

File myFile;
bool sdCardInitialized = false;
RTC_DS3231 rtc;

String antennaIn = "-----";
String antennaOut = "-----";
unsigned long lastReadIn = 0;
unsigned long lastReadOut = 0;

String processRFID(String raw) {
  String reversedCleanHex = "";
  for (int i = raw.length() - 1; i >= 0; i--) {
    char c = raw.charAt(i);
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
      reversedCleanHex += c;
    }
  }

  String hexa12 = (reversedCleanHex.length() >= 12) ? reversedCleanHex.substring(reversedCleanHex.length() - 12) : reversedCleanHex;
  String hexa10 = (hexa12.length() >= 10) ? hexa12.substring(hexa12.length() - 10) : hexa12;

  unsigned long long decimalVal = 0;
  for (int i = 0; i < hexa10.length(); i++) {
    char c = hexa10.charAt(i);
    int val = (c >= '0' && c <= '9') ? c - '0' :
              (c >= 'A' && c <= 'F') ? c - 'A' + 10 :
              (c >= 'a' && c <= 'f') ? c - 'a' + 10 : 0;
    decimalVal = decimalVal * 16 + val;
  }

  String decimalStr = "";
  if (decimalVal == 0) {
    decimalStr = "0";
  } else {
    unsigned long long tempVal = decimalVal;
    while (tempVal > 0) {
      decimalStr = (char)(tempVal % 10 + '0') + decimalStr;
      tempVal /= 10;
    }
  }

  return (decimalStr.length() >= 5) ? decimalStr.substring(decimalStr.length() - 5) : decimalStr;
}

void logRFID(String rfidName, String rfidData, String processedID, DateTime now) {
  String timestamp = String(now.month()) + "/" + String(now.day()) + "/" + String(now.year()) + "-" +
                     String(now.hour()) + ":" + (now.minute() < 10 ? "0" : "") + String(now.minute()) + ":" + (now.second() < 10 ? "0" : "") + String(now.second());

  Serial.print("Timestamp: ");
  Serial.print(timestamp);
  Serial.print(", Reader: ");
  Serial.print(rfidName);
  Serial.print(", Raw RFID: ");
  Serial.print(rfidData);
  Serial.print(", Processed RFID: ");
  Serial.println(processedID);

  if (sdCardInitialized) {
    myFile = SD.open("RFID_Log.csv", FILE_WRITE);
    if (myFile) {
      myFile.print(timestamp); myFile.print(",");
      myFile.print(rfidName); myFile.print(",");
      myFile.print(rfidData); myFile.print(",");
      myFile.println(processedID);
      myFile.close();
      Serial.println("Data written to Onboard SD card.");
    } else {
      Serial.println("Error opening RFID_Log.csv for writing. Data not logged to SD.");
    }
  } else {
    Serial.println("SD card not initialized. Data not logged to SD.");
  }

  if (rfidName == "RFID #1") {
    antennaIn = processedID;
    lastReadIn = millis();
  } else {
    antennaOut = processedID;
    lastReadOut = millis();
  }
}

void updateLCD(DateTime now) {
  if (!lcdPresent) return;

  lcd.clear();
  lcd.setCursor(0, 0);
  if (now.month() < 10) lcd.print("0");
  lcd.print(now.month());
  lcd.print("/");
  if (now.day() < 10) lcd.print("0");
  lcd.print(now.day());
  lcd.print("/");
  lcd.print(now.year());
  lcd.print("-");

  if (now.hour() < 10) lcd.print("0");
  lcd.print(now.hour());
  lcd.print(":");
  if (now.minute() < 10) lcd.print("0");
  lcd.print(now.minute());
  lcd.print(":");
  if (now.second() < 10) lcd.print("0");
  lcd.print(now.second());

  lcd.setCursor(0, 1);
  lcd.print("IN:  ");
  lcd.print(antennaIn);

  lcd.setCursor(0, 2);
  lcd.print("OUT: ");
  lcd.print(antennaOut);

  lcd.setCursor(0, 3);
  lcd.print("Dir: ");
  if (lastReadIn == 0 || lastReadOut == 0) {
    lcd.print("UNKNOWN");
  } else if (lastReadIn < lastReadOut) {
    lcd.print("IN");
  } else if (lastReadOut < lastReadIn) {
    lcd.print("OUT");
  } else {
    lcd.print("--");
  }
}

void checkRFID(HardwareSerial& serial, String label) {
  if (serial.available()) {
    String rfidData = "";
    while (serial.available()) {
      char c = serial.read();
      rfidData += c;
      delay(2);
    }
    rfidData.trim();
    String processed = processRFID(rfidData);

    DateTime now;
    if (rtc.begin(&myWire2)) {
      now = rtc.now();
    } else {
      now = DateTime(0, 0, 0, 0, 0, 0);
      Serial.println("RTC not available, using default timestamp.");
    }

    logRFID(label, rfidData, processed, now);
    updateLCD(now);
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println("Starting Serial Communication...");

  Serial4.begin(9600);
  Serial.println("RFID #1 Serial Ready (Pin 16)...");

  Serial3.begin(9600);
  Serial.println("RFID #2 Serial Ready (Pin 15)...");

  Serial.print("Initializing Onboard SD card...");
  if (!SD.begin(BUILTIN_SDCARD)) {
    Serial.println("Card failed or not present. RFID data will still be displayed on Serial Monitor.");
    sdCardInitialized = false;
  } else {
    Serial.println("Onboard SD card initialized.");
    sdCardInitialized = true;

    myFile = SD.open("RFID_Log.csv", FILE_WRITE);
    if (myFile) {
      if (myFile.size() == 0) {
        myFile.println("Timestamp,RFID Reader,Raw RFID,Processed RFID");
      }
      myFile.close();
      Serial.println("RFID_Log.csv opened and header checked.");
    } else {
      Serial.println("Error opening RFID_Log.csv for initial write. SD card logging might not work.");
      sdCardInitialized = false;
    }
  }

  Serial.println("Starting RTC Module and Setting Time");
  myWire2.begin(); // For RTC on pins 24 SDA2, 25 SCL2
  Wire.begin();    // For LCD on default pins 18 SDA, 19 SCL

  if (!rtc.begin(&myWire2)) {
    Serial.println("Couldn't find RTC on Wire2. Time will not be logged.");
    Serial.flush();
  } else {
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Initialize LCD if connected
  Wire.beginTransmission(LCD_I2C_ADDRESS);
  if (Wire.endTransmission() == 0) {
    delay(100);               // Give LCD backpack time to stabilize
    lcd.init();               // Initialize with LiquidCrystal_I2C
    lcd.backlight();          // Turn on backlight
    lcd.clear();
    lcdPresent = true;
    lcd.setCursor(0, 0);
    lcd.print("LCD Initialized");
  } else {
    lcdPresent = false;
    Serial.println("LCD not found on Wire.");
  }

  Serial.println("Ready to read RFID from both readers...");
}

void loop() {
  checkRFID(Serial4, "RFID #1");
  checkRFID(Serial3, "RFID #2");
}
