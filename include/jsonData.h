#pragma once
#include <NTPClient.h>
#include <ArduinoJson.h>

enum TrayName
{
    TRAY_MID = 0,
    TRAY_BOTTOM,
    TRAY_TOP,
    TRAY_4,
    TRAY_MAX
};

constexpr const char *trayNames[] = {"Moisture_Mid", "Moisture_Bottom", "Moisture_Top", "Moisture_4", "Moisture_MAX"};

struct TrayData
{
    uint16_t moisture = 0;
};
inline bool operator==(const TrayData &lhs, const TrayData &rhs) { return (lhs.moisture == rhs.moisture); }
inline bool operator!=(const TrayData &lhs, const TrayData &rhs) { return !(lhs == rhs); }

struct ReservoirData
{
    uint8_t level = 0;
    long rawData = 0;
};
inline bool operator==(const ReservoirData &lhs, const ReservoirData &rhs) { return ((lhs.level == rhs.level) &&
                                                                                     lhs.rawData == rhs.rawData); }
inline bool operator!=(const ReservoirData &lhs, const ReservoirData &rhs) { return !(lhs == rhs); }


enum PumpName
{
    PUMP_MID = 0,
    PUMP_BOTTOM,
    PUMP_TOP,
    PUMP_4,
    PUMP_MAX
};
constexpr const char *pumpNames[] = {"Duty_Mid", "Duty_Bottom", "Duty_Top", "Duty_4", "Duty_MAX"};

struct PumpData
{
    unsigned long accumulatedTime = 0;
    uint8_t duty = 0;
};
inline bool operator==(const PumpData &lhs, const PumpData &rhs) { return ((lhs.accumulatedTime == rhs.accumulatedTime) &&
                                                                           (lhs.duty == rhs.duty)); }
inline bool operator!=(const PumpData &lhs, const PumpData &rhs) { return !(lhs == rhs); }

class JsonData
{
    public:
    explicit JsonData() {}
    ReservoirData reservoir{};
    PumpData pumps[PUMP_MAX] {};
    TrayData trays[TRAY_MAX] {};

    void serializeIotPlotter(String &jsonString)
    {
        mJsonDoc.clear();
        auto object = mJsonDoc["data"].to<JsonObject>();
        object["Reservoir"][0]["value"] = reservoir.level;
        for (int i = 0; i < PUMP_MAX; i++)
        {
            object[pumpNames[i]][0]["value"] = pumps[i].duty;
        }
        for (int i = 0; i < TRAY_MAX; i++)
        {
            object[trayNames[i]][0]["value"] = trays[i].moisture;
        }
        serializeJson(mJsonDoc, jsonString);
    }

    void serializeWebServer(bool enabled, const String& updateTime, String &jsonString)
    {
        mJsonDoc.clear();
        mJsonDoc["enabled"] = enabled;
        mJsonDoc["rLevel"] = reservoir.level;
        mJsonDoc["rRaw"] = reservoir.rawData;
        mJsonDoc["p1AccTime"] = pumps[0].accumulatedTime;
        mJsonDoc["p1Duty"] = pumps[0].duty;
        mJsonDoc["p2AccTime"] = pumps[1].accumulatedTime;
        mJsonDoc["p2Duty"] = pumps[1].duty;
        mJsonDoc["p3AccTime"] = pumps[2].accumulatedTime;
        mJsonDoc["p3Duty"] = pumps[2].duty;
        mJsonDoc["p4AccTime"] = pumps[3].accumulatedTime;
        mJsonDoc["p4Duty"] = pumps[3].duty;
        mJsonDoc["t1Moisture"] = trays[0].moisture;
        mJsonDoc["t2Moisture"] = trays[1].moisture;
        mJsonDoc["t3Moisture"] = trays[2].moisture;
        mJsonDoc["t4Moisture"] = trays[3].moisture;
        mJsonDoc["lastUpdate"] = updateTime;

        serializeJson(mJsonDoc, jsonString);
    }

    private:
    JsonDocument mJsonDoc;
};
inline bool operator==(const JsonData &lhs, const JsonData &rhs) { return lhs.reservoir == rhs.reservoir &&
                                                                          lhs.pumps[0] == rhs.pumps[0] &&
                                                                          lhs.pumps[1] == rhs.pumps[1] &&
                                                                          lhs.pumps[2] == rhs.pumps[2] &&
                                                                          lhs.pumps[3] == rhs.pumps[3] &&
                                                                          lhs.trays[0] == rhs.trays[0] &&
                                                                          lhs.trays[1] == rhs.trays[1] &&
                                                                          lhs.trays[2] == rhs.trays[2] &&
                                                                          lhs.trays[3] == rhs.trays[3];}
inline bool operator!=(const JsonData &lhs, const JsonData &rhs) { return !(lhs == rhs); }

class IODevice
{
public:
    IODevice(NTPClient *ntpClient) : mNTPClient(ntpClient) {}
    virtual ~IODevice() = default;

    virtual void begin() = 0;
    virtual void fillData(JsonData &data) = 0;
    virtual void update() = 0;

protected:
    NTPClient *mNTPClient;
};