#pragma once

#include <stdint.h>
#include "pins.h"

class Pump
{
public:
    enum Direction
    {
        FORWARD = 0,
        BACKWARD = 1
    };

    enum Channel
    {
        PUMP_1 = 0,
        PUMP_2 = 1
    };

    Pump() = default;
    ~Pump() = default;

    void begin();

    uint32_t getDuty(Channel channel) const;
    void setDuty(Channel channel, uint8_t duty);

    void setMotorDirection(Channel channel, Direction direction);

private:
    static void setupPin(ConfiguredPin pin, Channel channel);

    // PWM Controls
    static constexpr uint8_t NUM_CHANNELS = 16;
    static constexpr uint8_t LEDC_TIMER_RESOLUTION = 8; // use 8 bit precission for LEDC timer
    static constexpr int LEDC_BASE_FREQ = 5000;

    uint32_t mPwmDuty[NUM_CHANNELS] = {0};
};