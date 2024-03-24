#pragma once

#include <stdint.h>
#include "jsonData.h"
#include "pins.h"

class Pump : public IODevice
{
public:
    Pump(ConfiguredPin enablePin, PumpNumber pumpNumber, NTPClient *ntpClient);
    ~Pump() = default;

    void begin() override;
    void fillData(JSONData &data) override;
    void update() override;
    
    uint8_t duty() const;
    void setDuty(uint8_t duty);
    
    void runPump(uint16_t duration, uint8_t duty);

private:
    void getData(PumpData &data) const;
    long getAccumulatedTime() const;

private:
    static void setupPin(ConfiguredPin pin, PumpNumber pumpNumber);

    // PWM Controls
    static constexpr uint8_t NUM_CHANNELS = 16;
    static constexpr uint8_t LEDC_TIMER_RESOLUTION = 8; // use 8 bit precission for LEDC timer
    static constexpr int LEDC_BASE_FREQ = 5000;

    ConfiguredPin mEnablePin;
    PumpNumber mPumpNumber;
    unsigned long mAccumulatedTime = 0;
    unsigned long mLastTime = 0;
    uint8_t mDuty = 0;

    unsigned long mCurrStartTime = 0;
    unsigned long mCurrEndTime = 0;
    uint8_t mCurrDuty = 0;
};