#pragma once

#include <stdint.h>
#include "jsonData.h"
#include "pins.h"

class Pump : public IODevice
{
public:
    Pump(ConfiguredPin enablePin, ConfiguredPin phasePin, PumpNumber pumpNumber, NTPClient *ntpClient);
    ~Pump() = default;

    void begin() override;
    void fillData(JSONData &data) override;
    void update() override;
    
    void runPump(uint16_t duration, uint8_t duty);

private:
    void getData(PumpData &data) const;
    long getAccumulatedTime() const;

    uint8_t getDuty() const;
    void setDuty(uint8_t duty);

private:
    static void setupPin(ConfiguredPin pin, PumpNumber pumpNumber);

    // PWM Controls
    static constexpr uint8_t NUM_CHANNELS = 16;
    static constexpr uint8_t LEDC_TIMER_RESOLUTION = 8; // use 8 bit precission for LEDC timer
    static constexpr int LEDC_BASE_FREQ = 5000;

    ConfiguredPin mEnablePin;
    ConfiguredPin mPhasePin;
    PumpNumber mPumpNumber;
    unsigned long mAccumulatedTime = 0;
    unsigned long mLastTime = 0;
    uint8_t mDuty = 0;

    unsigned long mCurrStartTime = 0;
    unsigned long mCurrEndTime = 0;
    uint8_t mCurrDuty = 0;
};