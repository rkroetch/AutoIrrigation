#pragma once
#include <Arduino.h>
#include <HCSR04.h>
#include <Smoothed.h>
#include "NTPClient.h"
#include "jsonData.h"
#include "pins.h"

class Reservoir : public IODevice
{
public:
    static constexpr float ROOM_TEMP = 20.5;
    static constexpr long DISTANCE_EMPTY = 650;
    static constexpr long DISTANCE_FULL = 200;

    Reservoir(NTPClient *ntpClient) : IODevice(ntpClient) {};
    ~Reservoir() = default;

    void begin() override;
    void fillData(JsonData &data) override;
    void update() override {};

private:
    void getData(ReservoirData& data);

private:
    Smoothed<long> mAverageRawData;
    uint8_t mErrorConsecutiveCount = 0;
};