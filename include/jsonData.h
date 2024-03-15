#pragma once
#include <NTPClient.h>

enum TrayNumber
{
    TRAY_1 = 0,
    TRAY_2,
    TRAY_3,
    TRAY_4,
    TRAY_MAX
};

struct TrayData
{
    uint16_t moisture = 0;
};
inline bool operator==(const TrayData &lhs, const TrayData &rhs) { return (lhs.moisture == rhs.moisture); }
inline bool operator!=(const TrayData &lhs, const TrayData &rhs) { return !(lhs == rhs); }

struct ReservoirData
{
    uint16_t level = 0;
    long rawData = 0;
};
inline bool operator==(const ReservoirData &lhs, const ReservoirData &rhs) { return ((lhs.level == rhs.level) &&
                                                                                     lhs.rawData == rhs.rawData); }
inline bool operator!=(const ReservoirData &lhs, const ReservoirData &rhs) { return !(lhs == rhs); }


enum PumpNumber
{
    PUMP_1 = 0,
    PUMP_2,
    PUMP_3,
    PUMP_4,
    PUMP_MAX
};
struct PumpData
{
    unsigned long accumulatedTime = 0;
    uint8_t duty = 0;
};
inline bool operator==(const PumpData &lhs, const PumpData &rhs) { return ((lhs.accumulatedTime == rhs.accumulatedTime) &&
                                                                           (lhs.duty == rhs.duty)); }
inline bool operator!=(const PumpData &lhs, const PumpData &rhs) { return !(lhs == rhs); }

struct JSONData
{
    ReservoirData reservoir{};
    PumpData pumps[PUMP_MAX] {};
    TrayData trays[TRAY_MAX] {};

};
inline bool operator==(const JSONData &lhs, const JSONData &rhs) { return lhs.reservoir == rhs.reservoir &&
                                                                          lhs.pumps[0] == rhs.pumps[0] &&
                                                                          lhs.pumps[1] == rhs.pumps[1] &&
                                                                          lhs.pumps[2] == rhs.pumps[2] &&
                                                                          lhs.pumps[3] == rhs.pumps[3] &&
                                                                          lhs.trays[0] == rhs.trays[0] &&
                                                                          lhs.trays[1] == rhs.trays[1] &&
                                                                          lhs.trays[2] == rhs.trays[2] &&
                                                                          lhs.trays[3] == rhs.trays[3];}
inline bool operator!=(const JSONData &lhs, const JSONData &rhs) { return !(lhs == rhs); }

class IODevice
{
public:
    IODevice(NTPClient *ntpClient) : mNTPClient(ntpClient) {}
    virtual ~IODevice() = default;

    virtual void begin() = 0;
    virtual void fillData(JSONData &data) = 0;
    virtual void update() = 0;

protected:
    NTPClient *mNTPClient;
};