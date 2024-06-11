#pragma once

#ifndef CONTROL_H
#define CONTROL_H

#if defined(ARDUINO) && ARDUINO >= 100
#include <Arduino.h>
#else
#include <WProgram.h>
#endif

#define COUNTIMER_MAX_HOURS 999
#define COUNTIMER_MAX_MINUTES_SECONDS 59

class Control
{
private:
    //Timer Side Variables
    uint16_t hours;
    uint8_t minutes;
    uint8_t seconds;
    uint32_t _interval = 1000;
    uint32_t _previousMillis;

    uint32_t _countTime;
    uint32_t _currentCountTime;
    uint32_t _startCountTime;

    void countDown();
    char _formatted_time[10];

    bool _isCounterCompleted;
    bool _isTimerCompleted;
    bool _isStopped;


    //Control Side Variables
    int _relay;
    int _vfd;
    int _vfdSpeed;
    int _pwm;
    bool _motorState;

public:
    Control(int relayPin);
    ~Control();

    //Setup
    void setTimer(char* hhmmss_str);
    //Run-Stop
    void run();
    void start();
    void stop();
    void resetTimer();

    //Get Time
    char* getTimeRemaining();
    uint16_t getCurrentHours();
    uint8_t getCurrentMinutes();
    uint8_t getCurrentSeconds();

    //Get Status
    bool isStopped();
    bool isTimerCompleted();

    //Relay Control
    void relayOn();
    void relayOff();
    bool getMotorState();
};

#endif