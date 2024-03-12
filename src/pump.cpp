#include "pump.h"
#include <Arduino.h>

void Pump::begin()
{
	pinMode(PIN_PUMP_1_ENABLE, OUTPUT);
	pinMode(PIN_PUMP_1_PHASE, OUTPUT);
	pinMode(PIN_PUMP_2_ENABLE, OUTPUT);
	pinMode(PIN_PUMP_2_PHASE, OUTPUT);

    setupPin(PIN_PUMP_1_ENABLE, PUMP_1);
    setupPin(PIN_PUMP_2_ENABLE, PUMP_2);
    setMotorDirection(PUMP_1, FORWARD);
    setMotorDirection(PUMP_2, FORWARD);
}

uint32_t Pump::getDuty(Channel channel) const
{
    if (channel < 0 || channel > NUM_CHANNELS)
        return 0;

    return mPwmDuty[channel];
}

void Pump::setDuty(Channel channel, uint8_t duty)
{
    if (channel < 0 || channel > NUM_CHANNELS)
        return;

    ledcWrite(channel, duty);
    mPwmDuty[channel] = duty;
}

void Pump::setMotorDirection(Channel motor, Direction direction)
{
    switch (motor)
    {
    case PUMP_1:
        digitalWrite(PIN_PUMP_1_PHASE, direction);
        break;
    case PUMP_2:
        digitalWrite(PIN_PUMP_2_PHASE, direction);
        break;
    default:
        break;
    }
}

void Pump::setupPin(ConfiguredPin pin, Channel channel)
{
    ledcSetup(channel, LEDC_BASE_FREQ, LEDC_TIMER_RESOLUTION);
    ledcAttachPin(pin, channel);
}