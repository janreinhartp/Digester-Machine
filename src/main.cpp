#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "RTClib.h"
#include <SPI.h>
#include <max6675.h>
#include "ezButton.h"
#include "control.h"
#include <EEPROMex.h>

// COMPONENTS DECLARATION
RTC_DS3231 rtc;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
#define CLOCK_INTERRUPT_PIN 19
uint8_t recentSec;
DateTime currentTime;
DateTime lastSave;
char alarm1String[12] = "hh:mm:ss";

// SD card
LiquidCrystal_I2C lcd(0x27, 20, 4);

int thermoDO = 8;
int thermoCS = 6;
int thermoCLK = 7;

MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);

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
int recentCount;

// Gas Meter
#define hall 5
ezButton sensor(hall);

// Data to save in SD Card
String dataToSave;

String compileData(DateTime TimeOfRecording, int PH, int TEMP, int PRESSURE, int COUNT)
{
  String DataToSave = String(TimeOfRecording.timestamp(DateTime::TIMESTAMP_FULL)) + "," +
                      String(PH) + "," +
                      String(TEMP) + "," +
                      String(PRESSURE) + "," +
                      String(COUNT);
  return DataToSave;
}

// |--------------------------------------------------------------------------------------------------------------------------------------------|
// |                                                         MENU                                                                               |
// |--------------------------------------------------------------------------------------------------------------------------------------------|

// Declaration of LCD Variables
const int NUM_MAIN_ITEMS = 4;
const int NUM_SETTING_ITEMS = 5;
const int NUM_TESTMACHINE_ITEMS = 4;

int currentMainScreen;
int currentSettingScreen;
int currentTestMenuScreen;
bool menuFlag, settingFlag, settingEditFlag, testMenuFlag, refreshScreen = false;

String menu_items[NUM_MAIN_ITEMS][2] = { // array with item names
    {"SETTING", "ENTER TO EDIT"},
    {"TEST MACHINE", "ENTER TO TEST"},
    {"RESET CURRENT COUNT", "ENTER TO RESET"},
    {"EXIT MENU", "ENTER TO RUN AUTO"}};

String setting_items[NUM_SETTING_ITEMS][2] = { // array with item names
    {"MOTOR RUN", "SEC"},
    {"SET TIME", "24 HR SETTING"},
    {"SET MAX PRESSURE", "PSI RELEASE"},
    {"SAVING INTERVAL", "MIN"},
    {"SAVE"}};

int parametersTimer[NUM_SETTING_ITEMS] = {1, 1, 1, 1};
int parametersTimerMaxValue[NUM_SETTING_ITEMS] = {1200, 24, 10, 60};

String testmachine_items[NUM_TESTMACHINE_ITEMS] = { // array with item names
    "MAIN CONTACTOR",
    "MOTOR RUN",
    "VALVE",
    "EXIT"};

int runTimeAdd = 20;
int setTimeAdd = 30;
int maxPresTimeAdd = 40;
int saveIntervalTimeAdd = 50;
int countAdd = 60;

Control ContactorVFD(A5);
Control RunVFD(A7);
Control GasValve(A6);

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

void saveCount(int CountCurrent)
{
  EEPROM.writeInt(countAdd, CountCurrent);
}
void readCount()
{
  int currentCountFromEEPROM = EEPROM.readInt(countAdd);
  Serial.println(currentCountFromEEPROM);
  sensor.setCount(currentCountFromEEPROM);
  recentCount = currentCountFromEEPROM;
}

void saveSettings()
{
  EEPROM.writeInt(runTimeAdd, parametersTimer[0]);
  EEPROM.writeInt(setTimeAdd, parametersTimer[1]);
  EEPROM.writeInt(maxPresTimeAdd, parametersTimer[2]);
  EEPROM.writeInt(saveIntervalTimeAdd, parametersTimer[3]);
}

void loadSettings()
{
  parametersTimer[0] = EEPROM.readInt(runTimeAdd);
  parametersTimer[1] = EEPROM.readInt(setTimeAdd);
  parametersTimer[2] = EEPROM.readInt(maxPresTimeAdd);
  parametersTimer[3] = EEPROM.readInt(saveIntervalTimeAdd);

  RunVFD.setTimer(secondsToHHMMSS(parametersTimer[0]));
}

void stopAll()
{
  ContactorVFD.stop();
  RunVFD.stop();
  GasValve.stop();
}

void setTimers()
{
  ContactorVFD.setTimer(secondsToHHMMSS(parametersTimer[0]));
  RunVFD.setTimer(secondsToHHMMSS(parametersTimer[0]));
  GasValve.setTimer(secondsToHHMMSS(60));
}
// Button Declaration
static const int buttonPin = 2;
int buttonStatePrevious = HIGH;

static const int buttonPin2 = 3;
int buttonStatePrevious2 = HIGH;

static const int buttonPin3 = 4;
int buttonStatePrevious3 = HIGH;

unsigned long minButtonLongPressDuration = 2000;
unsigned long buttonLongPressUpMillis;
unsigned long buttonLongPressDownMillis;
unsigned long buttonLongPressEnterMillis;
bool buttonStateLongPressUp = false;
bool buttonStateLongPressDown = false;
bool buttonStateLongPressEnter = false;

const int intervalButton = 50;
unsigned long previousButtonMillis;
unsigned long buttonPressDuration;
unsigned long currentMillis;

const int intervalButton2 = 50;
unsigned long previousButtonMillis2;
unsigned long buttonPressDuration2;
unsigned long currentMillis2;

const int intervalButton3 = 50;
unsigned long previousButtonMillis3;
unsigned long buttonPressDuration3;
unsigned long currentMillis3;

void InitializeButtons()
{
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(buttonPin2, INPUT_PULLUP);
  pinMode(buttonPin3, INPUT_PULLUP);
}

void readButtonUpState()
{
  if (currentMillis - previousButtonMillis > intervalButton)
  {
    int buttonState = digitalRead(buttonPin);
    if (buttonState == LOW && buttonStatePrevious == HIGH && !buttonStateLongPressUp)
    {
      buttonLongPressUpMillis = currentMillis;
      buttonStatePrevious = LOW;
    }
    buttonPressDuration = currentMillis - buttonLongPressUpMillis;
    if (buttonState == LOW && !buttonStateLongPressUp && buttonPressDuration >= minButtonLongPressDuration)
    {
      buttonStateLongPressUp = true;
    }
    if (buttonStateLongPressUp == true)
    {
      // Insert Fast Scroll Up
      if (menuFlag == true)
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
              parametersTimer[currentSettingScreen] += 1;
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
    }

    if (buttonState == HIGH && buttonStatePrevious == LOW)
    {
      buttonStatePrevious = HIGH;
      buttonStateLongPressUp = false;
      if (buttonPressDuration < minButtonLongPressDuration)
      {
        // Short Scroll Up
        if (menuFlag == true)
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
                parametersTimer[currentSettingScreen] += 1;
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
      }
    }
    previousButtonMillis = currentMillis;
  }
}

void readButtonDownState()
{
  if (currentMillis2 - previousButtonMillis2 > intervalButton2)
  {
    int buttonState2 = digitalRead(buttonPin2);
    if (buttonState2 == LOW && buttonStatePrevious2 == HIGH && !buttonStateLongPressDown)
    {
      buttonLongPressDownMillis = currentMillis2;
      buttonStatePrevious2 = LOW;
    }
    buttonPressDuration2 = currentMillis2 - buttonLongPressDownMillis;
    if (buttonState2 == LOW && !buttonStateLongPressDown && buttonPressDuration2 >= minButtonLongPressDuration)
    {
      buttonStateLongPressDown = true;
    }
    if (buttonStateLongPressDown == true)
    {
      if (menuFlag == true)
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
              parametersTimer[currentSettingScreen] -= 1;
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
    }

    if (buttonState2 == HIGH && buttonStatePrevious2 == LOW)
    {
      buttonStatePrevious2 = HIGH;
      buttonStateLongPressDown = false;
      if (buttonPressDuration2 < minButtonLongPressDuration)
      {
        if (menuFlag == true)
        {
          refreshScreen = true;
          if (settingFlag == true)
          {
            if (settingEditFlag == true)
            {
              if (currentSettingScreen == 2)
              {
                if (parametersTimer[currentSettingScreen] <= 2)
                {
                  parametersTimer[currentSettingScreen] = 2;
                }
                else
                {
                  parametersTimer[currentSettingScreen] -= 1;
                }
              }
              else
              {
                if (parametersTimer[currentSettingScreen] <= 0)
                {
                  parametersTimer[currentSettingScreen] = 0;
                }
                else
                {
                  parametersTimer[currentSettingScreen] -= 1;
                }
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
      }
    }
    previousButtonMillis2 = currentMillis2;
  }
}

void readButtonEnterState()
{
  if (currentMillis3 - previousButtonMillis3 > intervalButton3)
  {
    int buttonState3 = digitalRead(buttonPin3);
    if (buttonState3 == LOW && buttonStatePrevious3 == HIGH && !buttonStateLongPressEnter)
    {
      buttonLongPressEnterMillis = currentMillis3;
      buttonStatePrevious3 = LOW;
    }
    buttonPressDuration3 = currentMillis3 - buttonLongPressEnterMillis;
    if (buttonState3 == LOW && !buttonStateLongPressEnter && buttonPressDuration3 >= minButtonLongPressDuration)
    {
      buttonStateLongPressEnter = true;
    }
    if (buttonStateLongPressEnter == true)
    {
      // Insert Fast Scroll Enter
      Serial.println("Long Press Enter");
      if (menuFlag == false)
      {
        refreshScreen = true;
        menuFlag = true;
      }
    }

    if (buttonState3 == HIGH && buttonStatePrevious3 == LOW)
    {
      buttonStatePrevious3 = HIGH;
      buttonStateLongPressEnter = false;
      if (buttonPressDuration3 < minButtonLongPressDuration)
      {
        if (menuFlag == true)
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
            }else if(currentMainScreen == 2){
              sensor.resetCount();
              saveCount(0);
            }
            else
            {
              menuFlag = false;
            }
          }
        }
      }
    }
    previousButtonMillis3 = currentMillis3;
  }
}

void ReadButtons()
{
  currentMillis = millis();
  currentMillis2 = millis();
  currentMillis3 = millis();
  readButtonEnterState();
  readButtonUpState();
  readButtonDownState();
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

void printRunAuto(int PH, int PRESSURE, int TEMP, int COUNT)
{
  lcd.clear();
  lcd.setCursor(0, 0);
  char buff[] = "hh:mm:ss DD-MM-YYYY";
  lcd.print(currentTime.toString(buff));
  lcd.setCursor(0, 1);
  char buff2[] = "hh:mm:ss DD-MM-YYYY";
  lcd.print(lastSave.toString(buff2));
  lcd.setCursor(0, 2);
  lcd.print("PH");
  lcd.setCursor(6, 2);
  lcd.print("TEMP");
  lcd.setCursor(11, 2);
  lcd.print("PSI");
  lcd.setCursor(16, 2);
  lcd.print("GAS");

  lcd.setCursor(0, 3);
  lcd.print(PH);
  lcd.setCursor(6, 3);
  lcd.print(TEMP);
  lcd.setCursor(11, 3);
  lcd.print(PRESSURE);
  lcd.setCursor(16, 3);
  lcd.print(COUNT);
}

void printScreens()
{
  if (menuFlag == false)
  {
    printRunAuto(ph, pressure, temp, sensor.getCount());
    Serial.println(compileData(currentTime, ph, temp, pressure, sensor.getCount()));
    refreshScreen = false;
  }
  else
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
        printTestScreen(testmachine_items[currentTestMenuScreen], "Status", ContactorVFD.getMotorState(), false);
        break;
      case 1:
        printTestScreen(testmachine_items[currentTestMenuScreen], "Status", RunVFD.getMotorState(), false);
        break;
      case 2:
        printTestScreen(testmachine_items[currentTestMenuScreen], "Status", GasValve.getMotorState(), false);
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

void ReadSensors()
{
  temp = int(rtc.getTemperature()); // Read Temp
  readPH();                         // Read PH
  ph = phConvertion(voltage);       // Convert Raw Value

  pressureValue = analogRead(pressureInput);                                                                  // reads value from input pin and assigns to variable
  pressureValue = ((pressureValue - pressureZero) * pressuretransducermaxPSI) / (pressureMax - pressureZero); // conversion equation to convert analog reading to psi
  pressure = pressureValue;                                                                                   // Read Pressure
}
bool releasePressureFlag = false;

void RunValveViaSensor()
{
  if (pressure >= parametersTimer[2])
  {
    releasePressureFlag = true;
  }

  if (pressure <= 2)
  {
    releasePressureFlag = false;
  }

  if (releasePressureFlag == true)
  {
    GasValve.relayOn();
  }
  else
  {
    GasValve.relayOff();
  }
}
bool runAgitatorFlag = false;
void RunAutoStart()
{
  RunVFD.run();
  if (RunVFD.isTimerCompleted() == true)
  {
    ContactorVFD.relayOff();
    runAgitatorFlag = false;
  }
  else
  {
    ContactorVFD.relayOn();
  }
}

void CheckTimeForMixing()
{
  if (runAgitatorFlag == false)
  {
    if (currentTime.hour() == parametersTimer[1] && currentTime.minute() == 0 && currentTime.second() == 0)
    {
      runAgitatorFlag = true;
      RunVFD.start();
    }
  }
  else
  {
    RunAutoStart();
  }
}

// |--------------------------------------------------------------------------------------------------------------------------------------------|
// |                                                         INITIALIZE METHOD                                                                  |
// |--------------------------------------------------------------------------------------------------------------------------------------------|
void onAlarm()
{
  Serial.println("Alarm occured!");
}

void SetAlarm()
{
  if (rtc.alarmFired(1))
  {
    DateTime now = rtc.now(); // Get the current time
    lastSave = rtc.now();
    char buff[] = "Alarm triggered at hh:mm:ss DDD, DD MMM YYYY";
    Serial.println(now.toString(buff));

    Serial2.println(compileData(now, ph, temp, pressure, sensor.getCount()));

    // Disable and clear alarm
    rtc.disableAlarm(1);
    rtc.clearAlarm(1);

    if (!rtc.setAlarm1(
            rtc.now() + TimeSpan(0, 0, parametersTimer[3], 0),
            DS3231_A1_Minute // this mode triggers the alarm when the seconds match. See Doxygen for other options
            ))
    {
      Serial.println("Error, alarm wasn't set!");
    }
    else
    {
      Serial.println("Alarm Set!");
    }
  }
}

void initializeRTC()
{
  if (!rtc.begin())
  {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1)
      delay(10);
  }

  // rtc.adjust(DateTime(F(__DATE__), F("10:59:30")));
  //  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  if (rtc.lostPower())
  {
    Serial.println("RTC lost power, let's set the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  rtc.disable32K();
  pinMode(CLOCK_INTERRUPT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(CLOCK_INTERRUPT_PIN), onAlarm, FALLING);
  rtc.disableAlarm(1);
  rtc.disableAlarm(2);
  rtc.clearAlarm(1);
  rtc.clearAlarm(2);

  rtc.writeSqwPinMode(DS3231_OFF);

  if (!rtc.setAlarm1(
          rtc.now() + TimeSpan(0, 0, parametersTimer[3], 0),
          DS3231_A1_Minute))
  {
    Serial.println("Error, alarm wasn't set!");
  }
  else
  {
    Serial.println("Alarm set");
  }
}
void initializeLCD()
{
  lcd.init(); // initialize the lcd
  lcd.backlight();
}
void initializeSensors()
{
  sensor.setDebounceTime(100);
  sensor.setCountMode(COUNT_FALLING);
}

void readGasSensor()
{
  sensor.loop();
  if (recentCount != int(sensor.getCount()))
  {
    DateTime now = rtc.now();
    int curCount = sensor.getCount();
    saveCount(curCount);
    recentCount = curCount;
    Serial2.println(compileData(now, ph, temp, pressure, recentCount));
  }
}

void RunRTC()
{
  currentTime = rtc.now();
  if (recentSec != currentTime.second())
  {
    recentSec = currentTime.second();
    Serial.print(currentTime.day(), DEC);
    Serial.print('/');
    Serial.print(currentTime.month(), DEC);
    Serial.print('/');
    Serial.print(currentTime.year(), DEC);
    Serial.print(" (");
    Serial.print(daysOfTheWeek[currentTime.dayOfTheWeek()]);
    Serial.print(") ");
    Serial.print(currentTime.hour(), DEC);
    Serial.print(':');
    Serial.print(currentTime.minute(), DEC);
    Serial.print(':');
    Serial.print(currentTime.second(), DEC);
    Serial.println();
    Serial.print("Temperature: ");
    Serial.print(rtc.getTemperature());
    Serial.println(" C");
    refreshScreen = true;
  }
}
// |--------------------------------------------------------------------------------------------------------------------------------------------|
// |                                                         SETUP START                                                                        |
// |--------------------------------------------------------------------------------------------------------------------------------------------|

void setup()
{
  Serial.begin(9600);
  Serial2.begin(9600);
  loadSettings();
  initializeRTC();
  initializeLCD();
  initializeSensors();
  InitializeButtons();
  // saveCount();
  readCount();
  Serial.print("Current Count" + String(sensor.getCount()));
  refreshScreen = true;
}

// |--------------------------------------------------------------------------------------------------------------------------------------------|
// |                                                         LOOP START                                                                         |
// |--------------------------------------------------------------------------------------------------------------------------------------------|

void loop()
{
  ReadButtons();
  ReadSensors();
  readGasSensor();
  if (menuFlag == false)
  {
    RunValveViaSensor();
    CheckTimeForMixing();
    RunRTC();
    SetAlarm();
  }
  // Printing to LCD
  if (refreshScreen == true)
  {
    printScreens();
  }
}