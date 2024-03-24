#include "pump.h"
#include <Arduino.h>

Pump::Pump(ConfiguredPin enablePin, PumpNumber pumpNumber, NTPClient *ntpClient)
    : IODevice(ntpClient), mEnablePin(enablePin), mPumpNumber(pumpNumber)
{
    assert(pumpNumber < NUM_CHANNELS);
}

void Pump::begin()
{
    pinMode(mEnablePin, OUTPUT);
    setupPin(mEnablePin, mPumpNumber);
    
    // Ensure we start with the pump off to match mDuty
    ledcWrite(mPumpNumber, 0);
}

void Pump::update()
{
    if (mCurrEndTime == 0)
        return;

    auto currTime = millis();
    if (currTime >= mCurrStartTime && currTime <= mCurrEndTime)
    {
        setDuty(mCurrDuty);
    }
    else
    {
        setDuty(0);
        mCurrEndTime = 0;
    }
}

uint8_t Pump::duty() const
{
    return mDuty;
}

void Pump::runPump(uint16_t duration, uint8_t duty)
{
    mCurrStartTime = millis();
    mCurrEndTime = mCurrStartTime + (duration * 1000);
    mCurrDuty = duty;
}

void Pump::fillData(JSONData &data)
{
    getData(data.pumps[mPumpNumber]);
}

void Pump::getData(PumpData &data) const
{
    data.accumulatedTime = getAccumulatedTime();
    data.duty = mDuty;
}

long Pump::getAccumulatedTime() const
{
    if (mDuty > 0)
    {
        return mAccumulatedTime + (millis() - mLastTime);
    }
    return mAccumulatedTime;
}

void Pump::setDuty(uint8_t duty)
{
    if (duty == mDuty)
        return;

    if (duty > 0)
    {
        if (mDuty == 0)
            mLastTime = millis();
    }
    else
    {
        mAccumulatedTime += (millis() - mLastTime);
    }
    ledcWrite(mPumpNumber, duty);
    mDuty = duty;
}

void Pump::setupPin(ConfiguredPin pin, PumpNumber pumpNumber)
{
    ledcSetup(pumpNumber, LEDC_BASE_FREQ, LEDC_TIMER_RESOLUTION);
    ledcAttachPin(pin, pumpNumber);
}