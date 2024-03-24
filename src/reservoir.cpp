#include "reservoir.h"
// #include <Smoothed.h>

void Reservoir::begin()
{
    mAverageRawData.begin(SMOOTHED_AVERAGE, 15);
    pinMode(PIN_DISTANCE_TRIGGER, OUTPUT);
    pinMode(PIN_DISTANCE_ECHO, INPUT);
    HCSR04.begin(PIN_DISTANCE_TRIGGER, PIN_DISTANCE_ECHO);
}

void Reservoir::fillData(JSONData &data)
{
    getData(data.reservoir);
}

void Reservoir::getData(ReservoirData &data)
{
    long *microseconds = HCSR04.measureMicroseconds();
    if (microseconds[0] < 0)
        ++mErrorConsecutiveCount;
    else
        mErrorConsecutiveCount = 0;

    // Ignore up to 5 consecutive errors
    if (mErrorConsecutiveCount > 5)
    {
        data.rawData = microseconds[0];
        data.level = 0;
        return;
    }

    mErrorConsecutiveCount = 0;
    const_cast<Reservoir *>(this)->mAverageRawData.add(microseconds[0]);
    data.rawData = const_cast<Reservoir *>(this)->mAverageRawData.get();

    if (data.rawData >= DISTANCE_EMPTY)
        data.level = 0;
    else if (data.rawData <= DISTANCE_FULL)
        data.level = 100;
    else
        data.level = 100 - (((data.rawData - DISTANCE_FULL) * 100) / (DISTANCE_EMPTY - DISTANCE_FULL));
}
