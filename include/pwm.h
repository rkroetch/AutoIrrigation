#pragma once

#include <stdint.h>
#include "pins.h"

class Pwm
{
public:
    enum Direction
    {
        FORWARD = 0,
        BACKWARD = 1
    };

    enum Channel
    {
        MOTOR_A = 0,
        MOTOR_B = 1
    };

    Pwm() = default;
    ~Pwm() = default;

    void begin();

    uint32_t getDuty(Channel channel) const;
    void setDuty(Channel channel, uint32_t duty);

    void setMotorDirection(Channel channel, Direction direction);

private:
    static void setupPin(ConfiguredPin pin, Channel channel);

    // PWM Controls
    static constexpr uint8_t NUM_CHANNELS = 16;
    static constexpr uint8_t LEDC_TIMER_RESOLUTION = 12; // use 12 bit precission for LEDC timer
    static constexpr int LEDC_BASE_FREQ = 5000;

    uint32_t mPwmDuty[NUM_CHANNELS] = {0};
};