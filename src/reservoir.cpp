#include "reservoir.h"

void Reservoir::begin()
{
    HCSR04.begin(TRIGGER_PIN, ECHO_PIN);
}

double Reservoir::getFullness() const
{
    double *distances = HCSR04.measureDistanceCm(ROOM_TEMP);
    if (distances[0] <= DISTANCE_EMPTY)
        return 0.0;

    if (distances[0] >= DISTANCE_FULL)
        return 100.0;

    return ((distances[0] - DISTANCE_EMPTY) / DISTANCE_FULL);
}