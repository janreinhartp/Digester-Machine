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
#define CLOCK_INTERRUPT_PIN 19

// SD card
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
const int NUM_TESTMACHINE_ITEMS = 4;

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

Control ContactorVFD(A5);
Control RunVFD(A6);
Control GasValve(A7);

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
void onAlarm()
{
  Serial.println("Alarm occured!");
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

  rtc.disable32K();
  // Making it so, that the alarm will trigger an interrupt
  pinMode(CLOCK_INTERRUPT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(CLOCK_INTERRUPT_PIN), onAlarm, FALLING);
  // set alarm 1, 2 flag to false (so alarm 1, 2 didn't happen so far)
  // if not done, this easily leads to problems, as both register aren't reset on reboot/recompile
  rtc.clearAlarm(1);
  rtc.clearAlarm(2);

  // stop oscillating signals at SQW Pin
  // otherwise setAlarm1 will fail
  rtc.writeSqwPinMode(DS3231_OFF);

  // turn off alarm 2 (in case it isn't off already)
  // again, this isn't done at reboot, so a previously set alarm could easily go overlooked
  rtc.disableAlarm(2);

  // schedule an alarm 10 seconds in the future
  if (!rtc.setAlarm1(
          rtc.now() + TimeSpan(0,0,2,0),
          DS3231_A1_Minute // this mode triggers the alarm when the seconds match. See Doxygen for other options
          ))
  {
    Serial.println("Error, alarm wasn't set!");
  }
  else
  {
    Serial.println("Alarm will happen in 10 seconds!");
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
  Serial.print("Temperature: ");
  Serial.print(rtc.getTemperature());
  Serial.println(" C");

  Serial.println();
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
  loadSettings();
  refreshScreen = true;
}

// |--------------------------------------------------------------------------------------------------------------------------------------------|
// |                                                         LOOP START                                                                         |
// |--------------------------------------------------------------------------------------------------------------------------------------------|

void loop()
{
  ReadButtons();
  if (refreshScreen == true)
  {
    printScreens();
  }
  char date[10] = "hh:mm:ss";
  rtc.now().toString(date);

  sampleCode();
  // the stored alarm value + mode
  DateTime alarm1 = rtc.getAlarm1();
  Ds3231Alarm1Mode alarm1mode = rtc.getAlarm1Mode();
  char alarm1Date[12] = "DD hh:mm:ss";
  alarm1.toString(alarm1Date);
  Serial.print(" [Alarm1: ");
  Serial.print(alarm1Date);
  Serial.print(", Mode: ");
  switch (alarm1mode)
  {
  case DS3231_A1_PerSecond:
    Serial.print("PerSecond");
    break;
  case DS3231_A1_Second:
    Serial.print("Second");
    break;
  case DS3231_A1_Minute:
    Serial.print("Minute");
    break;
  case DS3231_A1_Hour:
    Serial.print("Hour");
    break;
  case DS3231_A1_Date:
    Serial.print("Date");
    break;
  case DS3231_A1_Day:
    Serial.print("Day");
    break;
  }

  // the value at SQW-Pin (because of pullup 1 means no alarm)
  Serial.print("] SQW: ");
  Serial.print(digitalRead(CLOCK_INTERRUPT_PIN));

  // whether a alarm fired
  Serial.print(" Fired: ");
  Serial.print(rtc.alarmFired(1));

  // Serial.print(" Alarm2: ");
  // Serial.println(rtc.alarmFired(2));
  // control register values (see https://datasheets.maximintegrated.com/en/ds/DS3231.pdf page 13)
  // Serial.print(" Control: 0b");
  // Serial.println(read_i2c_register(DS3231_ADDRESS, DS3231_CONTROL), BIN);

  // resetting SQW and alarm 1 flag
  // using setAlarm1, the next alarm could now be configurated
  if (rtc.alarmFired(1))
  {
    rtc.clearAlarm(1);
    Serial.print(" - Alarm cleared");
    if (!rtc.setAlarm1(
            rtc.now() + TimeSpan(0,0,2,0),
            DS3231_A1_Minute // this mode triggers the alarm when the seconds match. See Doxygen for other options
            ))
    {
      Serial.println("Error, alarm wasn't set!");
    }
    else
    {
      Serial.println("Alarm will happen in 10 seconds!");
    }
  }
  Serial.println();

  delay(1000);
}

// |--------------------------------------------------------------------------------------------------------------------------------------------|
// |                                                         SAMPLE CODES                                                                       |
// |--------------------------------------------------------------------------------------------------------------------------------------------|
