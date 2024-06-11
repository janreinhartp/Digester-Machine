#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "RTClib.h"
#include <SPI.h>
#include <SD.h>

LiquidCrystal_I2C lcd(0x27, 20, 4); // set the LCD address to 0x27 for a 16 chars and 2 line display

RTC_DS3231 rtc;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

const int chipSelect = 7;
File myFile;

void setup()
{
  Serial.begin(9600);
  lcd.init(); // initialize the lcd
  // Print a message to the LCD.
  lcd.backlight();
  lcd.setCursor(3, 0);
  lcd.print("Hello, world!");
  lcd.setCursor(2, 1);
  lcd.print("Ywrobot Arduino!");
  lcd.setCursor(0, 2);
  lcd.print("Arduino LCM IIC 2004");
  lcd.setCursor(2, 3);
  lcd.print("Power By Ec-yuan!");

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }

  Serial.print("Initializing SD card...");

  if (!SD.begin(chipSelect)) {
    Serial.println("initialization failed. Things to check:");
    Serial.println("1. is a card inserted?");
    Serial.println("2. is your wiring correct?");
    Serial.println("3. did you change the chipSelect pin to match your shield or module?");
    Serial.println("Note: press reset button on the board and reopen this Serial Monitor after fixing your issue!");
    while (true);
  }

  Serial.println("initialization done.");

  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  myFile = SD.open("test.txt", FILE_WRITE);

  // if the file opened okay, write to it:
  if (myFile) {
    Serial.print("Writing to test.txt...");
    myFile.println("testing 1, 2, 3.");
    // close the file:
    myFile.close();
    Serial.println("done.");
  } else {
    // if the file didn't open, print an error:
    Serial.println("error opening test.txt");
  }

  // re-open the file for reading:
  myFile = SD.open("test.txt");
  if (myFile) {
    Serial.println("test.txt:");

    // read from the file until there's nothing else in it:
    while (myFile.available()) {
      Serial.write(myFile.read());
    }
    // close the file:
    myFile.close();
  } else {
    // if the file didn't open, print an error:
    Serial.println("error opening test.txt");
  }
}

void loop()
{
  //  DateTime now = rtc.now();

  //   Serial.print(now.year(), DEC);
  //   Serial.print('/');
  //   Serial.print(now.month(), DEC);
  //   Serial.print('/');
  //   Serial.print(now.day(), DEC);
  //   Serial.print(" (");
  //   Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
  //   Serial.print(") ");
  //   Serial.print(now.hour(), DEC);
  //   Serial.print(':');
  //   Serial.print(now.minute(), DEC);
  //   Serial.print(':');
  //   Serial.print(now.second(), DEC);
  //   Serial.println();

  //   Serial.print(" since midnight 1/1/1970 = ");
  //   Serial.print(now.unixtime());
  //   Serial.print("s = ");
  //   Serial.print(now.unixtime() / 86400L);
  //   Serial.println("d");

  //   // calculate a date which is 7 days, 12 hours, 30 minutes, 6 seconds into the future
  //   DateTime future (now + TimeSpan(7,12,30,6));

  //   Serial.print(" now + 7d + 12h + 30m + 6s: ");
  //   Serial.print(future.year(), DEC);
  //   Serial.print('/');
  //   Serial.print(future.month(), DEC);
  //   Serial.print('/');
  //   Serial.print(future.day(), DEC);
  //   Serial.print(' ');
  //   Serial.print(future.hour(), DEC);
  //   Serial.print(':');
  //   Serial.print(future.minute(), DEC);
  //   Serial.print(':');
  //   Serial.print(future.second(), DEC);
  //   Serial.println();

  //   Serial.print("Temperature: ");
  //   Serial.print(rtc.getTemperature());
  //   Serial.println(" C");

  //   Serial.println();
  //   delay(3000);
}