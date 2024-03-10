#include "pwm.h"
#include <Arduino.h>

void Pwm::begin()
{
	pinMode(PIN_MOTOR_A_ENABLE, OUTPUT);
	pinMode(PIN_MOTOR_A_PHASE, OUTPUT);
	pinMode(PIN_MOTOR_B_ENABLE, OUTPUT);
	pinMode(PIN_MOTOR_B_PHASE, OUTPUT);

    setupPin(PIN_MOTOR_A_ENABLE, MOTOR_A);
    setupPin(PIN_MOTOR_B_ENABLE, MOTOR_B);
    setMotorDirection(MOTOR_A, FORWARD);
    setMotorDirection(MOTOR_B, FORWARD);
}

uint32_t Pwm::getDuty(Channel channel) const
{
    if (channel < 0 || channel > NUM_CHANNELS)
        return 0;

    return mPwmDuty[channel];
}

void Pwm::setDuty(Channel channel, uint32_t duty)
{
    if (channel < 0 || channel > NUM_CHANNELS)
        return;

    ledcWrite(channel, duty);
    mPwmDuty[channel] = duty;
}

void Pwm::setMotorDirection(Channel motor, Direction direction)
{
    switch (motor)
    {
    case MOTOR_A:
        digitalWrite(PIN_MOTOR_A_PHASE, direction);
        break;
    case MOTOR_B:
        digitalWrite(PIN_MOTOR_B_PHASE, direction);
        break;
    default:
        break;
    }
}

void Pwm::setupPin(ConfiguredPin pin, Channel channel)
{
    ledcSetup(channel, LEDC_BASE_FREQ, LEDC_TIMER_RESOLUTION);
    ledcAttachPin(pin, channel);
}