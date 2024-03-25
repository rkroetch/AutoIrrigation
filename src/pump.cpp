#include "pump.h"
#include <Arduino.h>

Pump::Pump(ConfiguredPin enablePin, PumpName pumpName, NTPClient *ntpClient)
    : IODevice(ntpClient), mEnablePin(enablePin), mPumpName(pumpName)
{
    assert(pumpName < NUM_CHANNELS);
}

void Pump::begin()
{
    pinMode(mEnablePin, OUTPUT);
    setupPin(mEnablePin, mPumpName);
    
    // Ensure we start with the pump off to match mDuty
    ledcWrite(mPumpName, 0);
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

void Pump::setEnabled(bool enabled)
{
    if (enabled == mEnabled)
        return;

    if (!enabled)
    {
        // Set duty to 0 before we set mEnabled.
        // setDuty will not update duty if mEnabled is false.
        setDuty(0);
        mCurrEndTime = 0;
    }
    mEnabled = enabled;
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

void Pump::fillData(JsonData &data)
{
    getData(data.pumps[mPumpName]);
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
    if (!mEnabled || (duty == mDuty))
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
    ledcWrite(mPumpName, duty);
    mDuty = duty;
}

void Pump::setupPin(ConfiguredPin pin, PumpName pumpName)
{
    ledcSetup(pumpName, LEDC_BASE_FREQ, LEDC_TIMER_RESOLUTION);
    ledcAttachPin(pin, pumpName);
}