#include "tray.h"
#include "pins.h"

Tray::Tray(ConfiguredPin moistureSensorPin, TrayNumber trayNumber, NTPClient *ntpClient) : IODevice(ntpClient), mMoistureSensorPin(moistureSensorPin), mTrayNumber(trayNumber)
{
}

void Tray::begin()
{
    // pinMode(mMoistureSensorPin, INPUT);
}

void Tray::fillData(JSONData &data)
{
    getData(data.trays[mTrayNumber]);
}

void Tray::update()
{
}

void Tray::getData(TrayData &data) const
{
    data.moisture = analogRead(mMoistureSensorPin);
}
