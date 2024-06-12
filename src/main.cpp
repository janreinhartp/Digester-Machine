#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "RTClib.h"
#include <SPI.h>
#include <SD.h>
#include "max6675.h"
#include "ezButton.h"
#include "control.h"
#include <EEPROMex.h>

// COMPONENTS DECLARATION
RTC_DS3231 rtc;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const int chipSelect = 7;
File myFile;
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Temp Sensor Declarations
int soPin = 50;  // SO=Serial Out
int csPin = 6;   // CS = chip select CS pin
int sckPin = 52; // SCK = Serial Clock
MAX6675 thermocouple(sckPin, csPin, soPin);

// PH Sensor Declarations
int pHSense = A0;
int samples = 10;
float adc_resolution = 1024.0;

// Pressure Sensor Declaration
const int pressureInput = A2;             // select the analog input pin for the pressure transducer
const int pressureZero = 102.4;           // analog reading of pressure transducer at 0psi
const int pressureMax = 921.6;            // analog reading of pressure transducer at 100psi
const int pressuretransducermaxPSI = 100; // psi value of transducer being used
float pressureValue = 0;                  // variable to store the value coming from the pressure transducer

// Variables for printing and saving
int temp;
int ph;
int pressure;
int measurings;
float voltage;

// Gas Meter
#define hall 5
ezButton sensor(hall);

// |--------------------------------------------------------------------------------------------------------------------------------------------|
// |                                                         MENU                                                                               |
// |--------------------------------------------------------------------------------------------------------------------------------------------|

// Declaration of LCD Variables
const int NUM_MAIN_ITEMS = 3;
const int NUM_SETTING_ITEMS = 5;
const int NUM_TESTMACHINE_ITEMS = 5;

int currentMainScreen;
int currentSettingScreen;
int currentTestMenuScreen;
bool menuFlag, settingFlag, settingEditFlag, testMenuFlag, refreshScreen = false;

String menu_items[NUM_MAIN_ITEMS][2] = { // array with item names
    {"SETTING", "ENTER TO EDIT"},
    {"TEST MACHINE", "ENTER TO TEST"},
    {"EXIT MENU", "ENTER TO RUN AUTO"}};

String setting_items[NUM_SETTING_ITEMS][2] = { // array with item names
    {"MOTOR RUN", "SEC"},
    {"SET TIME", "24 HR SETTING"},
    {"SET MAX PRESSURE", "PSI RELEASE"},
    {"SAVING INTERVAL", "MIN"},
    {"SAVE"}};

int parametersTimer[NUM_SETTING_ITEMS] = {1, 1, 1, 1};
int parametersTimerMaxValue[NUM_SETTING_ITEMS] = {1200, 24, 15, 60};

String testmachine_items[NUM_TESTMACHINE_ITEMS] = { // array with item names
    "MAIN CONTACTOR",
    "MOTOR RUN",
    "VALVE",
    "EXIT"};

int runTimeAdd = 20;
int setTimeAdd = 30;
int maxPresTimeAdd = 40;
int saveIntervalTimeAdd = 40;

void saveSettings()
{
  EEPROM.writeDouble(runTimeAdd, parametersTimer[0]);
  EEPROM.writeDouble(setTimeAdd, parametersTimer[1]);
  EEPROM.writeDouble(maxPresTimeAdd, parametersTimer[2]);
  EEPROM.writeDouble(saveIntervalTimeAdd, parametersTimer[3]);
}

void loadSettings()
{
  parametersTimer[0] = EEPROM.readDouble(runTimeAdd);
  parametersTimer[1] = EEPROM.readDouble(setTimeAdd);
  parametersTimer[2] = EEPROM.readDouble(maxPresTimeAdd);
  parametersTimer[3] = EEPROM.readDouble(saveIntervalTimeAdd);
}

char *secondsToHHMMSS(int total_seconds)
{
  int hours, minutes, seconds;

  hours = total_seconds / 3600;         // Divide by number of seconds in an hour
  total_seconds = total_seconds % 3600; // Get the remaining seconds
  minutes = total_seconds / 60;         // Divide by number of seconds in a minute
  seconds = total_seconds % 60;         // Get the remaining seconds

  // Format the output string
  static char hhmmss_str[7]; // 6 characters for HHMMSS + 1 for null terminator
  sprintf(hhmmss_str, "%02d%02d%02d", hours, minutes, seconds);
  return hhmmss_str;
}

Control ContactorVFD(100);
Control RunVFD(100);
Control GasValve(100);

void stopAll()
{
  ContactorVFD.stop();
  RunVFD.stop();
  GasValve.stop();
}

void setTimers()
{
  ContactorVFD.setTimer(secondsToHHMMSS(parametersTimer[0]));
  RunVFD.setTimer(secondsToHHMMSS(60));
  GasValve.setTimer(secondsToHHMMSS(60));
}

// Buttons
ezButton prevButton(7);
ezButton nextButton(7);
ezButton enterButton(7);

const int LONG_PRESS_TIME = 5000; // 1000 milliseconds
unsigned long pressedTime = 0;
unsigned long releasedTime = 0;

void InitializeButtons()
{
  prevButton.setDebounceTime(100);
  nextButton.setDebounceTime(100);
  enterButton.setDebounceTime(100);
}
void ReadButtons()
{
  prevButton.loop();
  nextButton.loop();
  enterButton.loop();

  if (menuFlag == false)
  {
    // Enter Menu
    if (prevButton.isPressed() && nextButton.isPressed() && enterButton.isPressed())
    {
      pressedTime = millis();
    }

    if (prevButton.isReleased() && nextButton.isReleased() && enterButton.isReleased())
    {
      releasedTime = millis();

      long pressDuration = releasedTime - pressedTime;

      if (pressDuration > LONG_PRESS_TIME)
      {
        menuFlag = true;
      }
    }
  }
  else if (menuFlag == true)
  {
    // ------- START PREV CLICK -------
    if (prevButton.isPressed())
    {
      refreshScreen = true;
      if (settingFlag == true)
      {
        if (settingEditFlag == true)
        {
          if (parametersTimer[currentSettingScreen] <= 0)
          {
            parametersTimer[currentSettingScreen] = 0;
          }
          else
          {
            parametersTimer[currentSettingScreen] -= 0.1;
          }
        }
        else
        {
          if (currentSettingScreen == 0)
          {
            currentSettingScreen = NUM_SETTING_ITEMS - 1;
          }
          else
          {
            currentSettingScreen--;
          }
        }
      }
      else if (testMenuFlag == true)
      {
        if (currentTestMenuScreen == 0)
        {
          currentTestMenuScreen = NUM_TESTMACHINE_ITEMS - 1;
        }
        else
        {
          currentTestMenuScreen--;
        }
      }
      else
      {
        if (currentMainScreen == 0)
        {
          currentMainScreen = NUM_MAIN_ITEMS - 1;
        }
        else
        {
          currentMainScreen--;
        }
      }
    }
    // ------- END PREV CLICK -------

    // ------- START NEXT CLICK -------
    if (nextButton.isPressed())
    {
      refreshScreen = true;
      if (settingFlag == true)
      {
        if (settingEditFlag == true)
        {
          if (parametersTimer[currentSettingScreen] >= parametersTimerMaxValue[currentSettingScreen] - 1)
          {
            parametersTimer[currentSettingScreen] = parametersTimerMaxValue[currentSettingScreen];
          }
          else
          {
            parametersTimer[currentSettingScreen] += 0.1;
          }
        }
        else
        {
          if (currentSettingScreen == NUM_SETTING_ITEMS - 1)
          {
            currentSettingScreen = 0;
          }
          else
          {
            currentSettingScreen++;
          }
        }
      }
      else if (testMenuFlag == true)
      {
        if (currentTestMenuScreen == NUM_TESTMACHINE_ITEMS - 1)
        {
          currentTestMenuScreen = 0;
        }
        else
        {
          currentTestMenuScreen++;
        }
      }
      else
      {
        if (currentMainScreen == NUM_MAIN_ITEMS - 1)
        {
          currentMainScreen = 0;
        }
        else
        {
          currentMainScreen++;
        }
      }
    }
    // ------- END NEXT CLICK -------

    // ------- START ENTER CLICK -------
    if (enterButton.isPressed())
    {
      refreshScreen = true;
      if (currentMainScreen == 0 && settingFlag == true)
      {
        if (currentSettingScreen == NUM_SETTING_ITEMS - 1)
        {
          settingFlag = false;
          saveSettings();
          loadSettings();
          currentSettingScreen = 0;
          setTimers();
        }
        else
        {
          if (settingEditFlag == true)
          {
            settingEditFlag = false;
          }
          else
          {
            settingEditFlag = true;
          }
        }
      }
      else if (currentMainScreen == 1 && testMenuFlag == true)
      {
        if (currentTestMenuScreen == NUM_TESTMACHINE_ITEMS - 1)
        {
          currentMainScreen = 0;
          currentTestMenuScreen = 0;
          testMenuFlag = false;
          stopAll();
        }
        else if (currentTestMenuScreen == 0)
        {
          if (ContactorVFD.getMotorState() == false)
          {
            ContactorVFD.relayOn();
          }
          else
          {
            ContactorVFD.relayOff();
          }
        }
        else if (currentTestMenuScreen == 1)
        {
          if (RunVFD.getMotorState() == false)
          {
            RunVFD.relayOn();
          }
          else
          {
            RunVFD.relayOff();
          }
        }
        else if (currentTestMenuScreen == 2)
        {
          if (GasValve.getMotorState() == false)
          {
            GasValve.relayOn();
          }
          else
          {
            GasValve.relayOff();
          }
        }
      }
      else
      {
        if (currentMainScreen == 0)
        {
          settingFlag = true;
        }
        else if (currentMainScreen == 1)
        {
          testMenuFlag = true;
        }
        else
        {
          menuFlag = false;
        }
      }
    }
    // ------- END ENTER CLICK -------
  }
}

// |--------------------------------------------------------------------------------------------------------------------------------------------|
// |                                                         INITIALIZE METHOD                                                                  |
// |--------------------------------------------------------------------------------------------------------------------------------------------|
void printMainMenu(String MenuItem, String Action)
{
  lcd.clear();
  lcd.print(MenuItem);
  lcd.setCursor(0, 3);
  lcd.write(0);
  lcd.setCursor(2, 3);
  lcd.print(Action);
  refreshScreen = false;
}

void printSettingScreen(String SettingTitle, String Unit, double Value, bool EditFlag, bool SaveFlag)
{
	lcd.clear();
	lcd.print(SettingTitle);
	lcd.setCursor(0, 1);

	if (SaveFlag == true)
	{
		lcd.setCursor(0, 3);
		lcd.write(0);
		lcd.setCursor(2, 3);
		lcd.print("ENTER TO SAVE ALL");
	}
	else
	{
		lcd.print(Value);
		lcd.print(" ");
		lcd.print(Unit);
		lcd.setCursor(0, 3);
		lcd.write(0);
		lcd.setCursor(2, 3);
		if (EditFlag == false)
		{
			lcd.print("ENTER TO EDIT");
		}
		else
		{
			lcd.print("ENTER TO SAVE");
		}
	}
	refreshScreen = false;
}

void printTestScreen(String TestMenuTitle, String Job, bool Status, bool ExitFlag)
{
	lcd.clear();
	lcd.print(TestMenuTitle);
	if (ExitFlag == false)
	{
		lcd.setCursor(0, 2);
		lcd.print(Job);
		lcd.print(" : ");
		if (Status == true)
		{
			lcd.print("ON");
		}
		else
		{
			lcd.print("OFF");
		}
	}

	if (ExitFlag == true)
	{
		lcd.setCursor(0, 3);
		lcd.print("Click to Exit Test");
	}
	else
	{
		lcd.setCursor(0, 3);
		lcd.print("Click to Run Test");
	}
	refreshScreen = false;
}


void printScreens()
{
  if (settingFlag == true)
  {
    if (currentSettingScreen == NUM_SETTING_ITEMS - 1)
    {
      printSettingScreen(setting_items[currentSettingScreen][0], setting_items[currentSettingScreen][1], parametersTimer[currentSettingScreen], settingEditFlag, true);
    }
    else
    {
      printSettingScreen(setting_items[currentSettingScreen][0], setting_items[currentSettingScreen][1], parametersTimer[currentSettingScreen], settingEditFlag, false);
    }
  }
  else if (testMenuFlag == true)
  {
    switch (currentTestMenuScreen)
    {
    case 0:
      printTestScreen(testmachine_items[currentTestMenuScreen], "", !ContactorVFD.isTimerCompleted(), false);
      break;
    case 1:
      printTestScreen(testmachine_items[currentTestMenuScreen], "", !RunVFD.isTimerCompleted(), false);
      break;
    case 2:
      printTestScreen(testmachine_items[currentTestMenuScreen], "", !GasValve.isTimerCompleted(), false);
      break;
    case 3:
      printTestScreen(testmachine_items[currentTestMenuScreen], "", true, true);
      break;

    default:
      break;
    }
  }
  else
  {
    printMainMenu(menu_items[currentMainScreen][0], menu_items[currentMainScreen][1]);
  }
}



// |--------------------------------------------------------------------------------------------------------------------------------------------|
// |                                                         READ SENSOR METHODS                                                                |
// |--------------------------------------------------------------------------------------------------------------------------------------------|

float phConvertion(float voltage)
{
  return 7 + ((2.5 - voltage) / 0.18);
}

void readPH()
{
  measurings = 0;

  for (int i = 0; i < samples; i++)
  {
    measurings += analogRead(pHSense);
    delay(10);
  }

  voltage = 5 / adc_resolution * measurings / samples;
}

void readSensors()
{
  temp = thermocouple.readCelsius(); // Read Temp
  readPH();                          // Read PH
  ph = phConvertion(voltage);        // Convert Raw Value

  pressureValue = analogRead(pressureInput);                                                                  // reads value from input pin and assigns to variable
  pressureValue = ((pressureValue - pressureZero) * pressuretransducermaxPSI) / (pressureMax - pressureZero); // conversion equation to convert analog reading to psi
  pressure = pressureValue;                                                                                   // Read Pressure
}



// |--------------------------------------------------------------------------------------------------------------------------------------------|
// |                                                         INITIALIZE METHOD                                                                  |
// |--------------------------------------------------------------------------------------------------------------------------------------------|
void initializeRTC()
{
  if (!rtc.begin())
  {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1)
      delay(10);
  }

  if (rtc.lostPower())
  {
    Serial.println("RTC lost power, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }
}
void initializeLCD()
{
  lcd.init(); // initialize the lcd
  // Print a message to the LCD.
  lcd.backlight();

  // lcd.setCursor(3, 0);
  // lcd.print("Hello, world!");
  // lcd.setCursor(2, 1);
  // lcd.print("Ywrobot Arduino!");
  // lcd.setCursor(0, 2);
  // lcd.print("Arduino LCM IIC 2004");
  // lcd.setCursor(2, 3);
  // lcd.print("Power By Ec-yuan!");
}
void initializeSensors()
{
  sensor.setDebounceTime(100);
  sensor.setCountMode(COUNT_FALLING);
}
// |--------------------------------------------------------------------------------------------------------------------------------------------|
// |                                                         SETUP START                                                                        |
// |--------------------------------------------------------------------------------------------------------------------------------------------|

void setup()
{
  Serial.begin(9600);
  initializeRTC();
  initializeLCD();
  initializeSensors();
  InitializeButtons();
}

// |--------------------------------------------------------------------------------------------------------------------------------------------|
// |                                                         LOOP START                                                                         |
// |--------------------------------------------------------------------------------------------------------------------------------------------|

void loop()
{
  ReadButtons();
  if(refreshScreen == true){
    printMainMenu();
  }
}

// |--------------------------------------------------------------------------------------------------------------------------------------------|
// |                                                         SAMPLE CODES                                                                       |
// |--------------------------------------------------------------------------------------------------------------------------------------------|
void sampleCode()
{
  DateTime now = rtc.now();

  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" (");
  Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
  Serial.print(") ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();

  Serial.print(" since midnight 1/1/1970 = ");
  Serial.print(now.unixtime());
  Serial.print("s = ");
  Serial.print(now.unixtime() / 86400L);
  Serial.println("d");

  // calculate a date which is 7 days, 12 hours, 30 minutes, 6 seconds into the future
  DateTime future(now + TimeSpan(7, 12, 30, 6));

  Serial.print(" now + 7d + 12h + 30m + 6s: ");
  Serial.print(future.year(), DEC);
  Serial.print('/');
  Serial.print(future.month(), DEC);
  Serial.print('/');
  Serial.print(future.day(), DEC);
  Serial.print(' ');
  Serial.print(future.hour(), DEC);
  Serial.print(':');
  Serial.print(future.minute(), DEC);
  Serial.print(':');
  Serial.print(future.second(), DEC);
  Serial.println();

  Serial.print("Temperature: ");
  Serial.print(rtc.getTemperature());
  Serial.println(" C");

  Serial.println();
}