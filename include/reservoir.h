#pragma once
#include <Arduino.h>
#include <HCSR04.h>
#include "pins.h"

class Reservoir
{
public:
    static constexpr uint8_t TRIGGER_PIN = PIN_DISTANCE_TRIGGER;
    static constexpr uint8_t ECHO_PIN = PIN_DISTANCE_ECHO;
    static constexpr float ROOM_TEMP = 20.5;
    static constexpr double DISTANCE_EMPTY = 300.0;
    static constexpr double DISTANCE_FULL = 100.0;

    Reservoir();

    void begin();
    double getFullness() const;

private:
};