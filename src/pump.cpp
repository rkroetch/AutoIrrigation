#include "pump.h"
#include <Arduino.h>

Pump::Pump(ConfiguredPin enablePin, ConfiguredPin phasePin, PumpNumber pumpNumber, NTPClient *ntpClient)
    : IODevice(ntpClient), mEnablePin(enablePin), mPhasePin(phasePin), mPumpNumber(pumpNumber)
{
    assert(pumpNumber < NUM_CHANNELS);
}

void Pump::begin()
{
    pinMode(mEnablePin, OUTPUT);
    pinMode(mPhasePin, OUTPUT);

    setupPin(mEnablePin, mPumpNumber);
    digitalWrite(mPhasePin, 1); // Temporary - this will be pulled high
    
    // Ensure we start with the pump off to match mDuty
    ledcWrite(mPumpNumber, 0);
}

void Pump::update()
{
    auto currTime = millis();
    if (currTime >= mCurrStartTime && currTime <= mCurrEndTime)
    {
        setDuty(mCurrDuty);
    }
    else
    {
        setDuty(0);
    }
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
    data.duty = getDuty();
}

long Pump::getAccumulatedTime() const
{
    if (mDuty > 0)
    {
        return mAccumulatedTime + (millis() - mLastTime);
    }
    return mAccumulatedTime;
}

uint8_t Pump::getDuty() const
{
    return mDuty;
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
        mAccumulatedTime += millis() - mLastTime;
    }
    ledcWrite(mPumpNumber, duty);
    mDuty = duty;
}

void Pump::setupPin(ConfiguredPin pin, PumpNumber pumpNumber)
{
    ledcSetup(pumpNumber, LEDC_BASE_FREQ, LEDC_TIMER_RESOLUTION);
    ledcAttachPin(pin, pumpNumber);
}