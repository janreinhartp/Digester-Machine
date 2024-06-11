#include "control.h"

Control::Control(int relayPin)
{
  _relay = relayPin;
  pinMode(_relay, OUTPUT);
  digitalWrite(_relay, HIGH);
  _previousMillis = 0;
  _currentCountTime = 0;
  _countTime = 0;
  _isCounterCompleted = true;
  _isStopped = true;
  _startCountTime = 0;
  _motorState = false;
}

Control::~Control()
{
}

void Control::setTimer(char *hhmmss_str)
{
  int hours, minutes, seconds;

  // Extract the hours, minutes, and seconds from the input string
  hours = (hhmmss_str[0] - '0') * 10 + (hhmmss_str[1] - '0');
  minutes = (hhmmss_str[2] - '0') * 10 + (hhmmss_str[3] - '0');
  seconds = (hhmmss_str[4] - '0') * 10 + (hhmmss_str[5] - '0');

  if (hours > COUNTIMER_MAX_HOURS)
  {
    hours = COUNTIMER_MAX_HOURS;
  }

  if (minutes > COUNTIMER_MAX_MINUTES_SECONDS)
  {
    minutes = COUNTIMER_MAX_MINUTES_SECONDS;
  }

  if (seconds > COUNTIMER_MAX_MINUTES_SECONDS)
  {
    seconds = COUNTIMER_MAX_MINUTES_SECONDS;
  }

  _currentCountTime = ((hours * 3600L) + (minutes * 60L) + seconds) * 1000L;
  _countTime = _currentCountTime;

  _startCountTime = _currentCountTime;
}

void Control::start()
{
  _isStopped = false;
  this->relayOn();
  if (_isCounterCompleted)
    _isCounterCompleted = false;
}

void Control::stop()
{
  _isStopped = true;
  this->relayOff();
  _isCounterCompleted = true;
  _currentCountTime = _countTime;
}

void Control::run()
{
  // timer is running only if is not completed or not stopped.
  if (_isCounterCompleted || _isStopped)
    return;

  if (millis() - _previousMillis >= _interval)
  {
    countDown();
    _previousMillis = millis();
  }
}

bool Control::isTimerCompleted()
{
  return _isCounterCompleted;
}

bool Control::isStopped()
{
  return _isStopped;
}

void Control::countDown()
{
  if (_currentCountTime > 0)
  {
    _currentCountTime -= _interval;
  }
  else
  {
    stop();
  }
}

char *Control::getTimeRemaining()
{
  sprintf(_formatted_time, "%02d:%02d:%02d", getCurrentHours(), getCurrentMinutes(), getCurrentSeconds());
  return _formatted_time;
}

uint16_t Control::getCurrentHours()
{
  return _currentCountTime / 1000 / 3600;
}

uint8_t Control::getCurrentMinutes()
{
  return _currentCountTime / 1000 % 3600 / 60;
}

uint8_t Control::getCurrentSeconds()
{
  return _currentCountTime / 1000 % 3600 % 60 % 60;
}

void Control::relayOn()
{
  _motorState = true;
  digitalWrite(_relay, LOW);
}
void Control::relayOff()
{
  _motorState = false;
  digitalWrite(_relay, HIGH);
}

bool Control::getMotorState()
{
  return _motorState;
}
