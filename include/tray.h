#pragma once
#include <cstdint>
#include <Arduino.h>
#include <Smoothed.h>
#include "jsonData.h"
#include "pins.h"

class Tray : public IODevice
{
public:
    Tray(ConfiguredPin moistureSensorPin, TrayName trayName, NTPClient *ntpClient);

    void begin() override;
    void fillData(JsonData &data) override;
    void update() override;

private:
    void getData(TrayData &data) const;

private:
    TrayName mTrayName;
    ConfiguredPin mMoistureSensorPin;
    Smoothed<uint16_t> mAverageData;
};