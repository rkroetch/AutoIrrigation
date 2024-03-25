#include "tray.h"
#include "pins.h"

Tray::Tray(ConfiguredPin moistureSensorPin, TrayName trayName, NTPClient *ntpClient) : IODevice(ntpClient), mMoistureSensorPin(moistureSensorPin), mTrayName(trayName)
{
}

void Tray::begin()
{
    mAverageData.begin(SMOOTHED_AVERAGE, 15);
}

void Tray::fillData(JsonData &data)
{
    getData(data.trays[mTrayName]);
}

void Tray::update()
{
}

void Tray::getData(TrayData &data) const
{
    const_cast<Tray *>(this)->mAverageData.add(analogRead(mMoistureSensorPin));
    data.moisture = const_cast<Tray *>(this)->mAverageData.get();
}
