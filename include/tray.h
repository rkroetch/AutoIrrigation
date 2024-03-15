#pragma once
#include "jsonData.h"
#include "pins.h"

class Tray : public IODevice
{
public:
    Tray(ConfiguredPin moistureSensorPin, TrayNumber trayNumber, NTPClient *ntpClient);

    void begin() override;
    void fillData(JSONData &data) override;
    void update() override;

private:
    void getData(TrayData &data) const;

private:
    TrayNumber mTrayNumber;
    ConfiguredPin mMoistureSensorPin;
};