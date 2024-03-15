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
    static constexpr double DISTANCE_EMPTY = 10.0;
    static constexpr double DISTANCE_FULL = 3.0;

    Reservoir(NTPClient *ntpClient) : IODevice(ntpClient) {};
    ~Reservoir() = default;

    void begin() override;
    void fillData(JSONData &data) override;
    void update() override {};

private:
    void getData(ReservoirData& data);

private:
    Smoothed<long> mAverageRawData;
    uint8_t mErrorConsecutiveCount = 0;
};