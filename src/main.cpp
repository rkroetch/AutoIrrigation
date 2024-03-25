#include <array>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <EEPROM.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <NTPClient.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiUdp.h>

#include <esp_task_wdt.h>

#include "pins.h"
#include "JSONData.h"
#include "reservoir.h"
#include "tray.h"
#include "pump.h"

/* =============== config section start =============== */

constexpr uint16_t HTTP_PORT = 8080;
constexpr uint8_t WDT_TIMEOUT = 30; // seconds

constexpr uint8_t EEPROM_SIZE = 1; // 1 byte storing global 'enable' flag

constexpr const char *hostName = "robowater.duckdns.org";
constexpr const char *SECRET_KEY = "tinkle";

constexpr uint8_t IOTPLOTTER_LOG_INTERVAL_M = 5;
constexpr const char *IOTPLOTTER_API_KEY = "c1480e3942bb07381027506997d5809decc9770008";
constexpr const char *IOTPLOTTER_URL = "http://iotplotter.com/api/v2/feed/851329303456179536";

#if __has_include("credentials.h")
#include "credentials.h"
#else
// WiFi credentials
#define NUM_NETWORKS 1
// Add your networks credentials here
constexpr const char *ssidTab[NUM_NETWORKS] = {
    "tinkle_IoT"};
constexpr const char *passwordTab[NUM_NETWORKS] = {
    "tinkle_key"};
#endif
/* =============== config section end =============== */

#define LOG(f_, ...)                        \
    {                                       \
        Serial.printf((f_), ##__VA_ARGS__); \
    }

// you can provide credentials to multiple WiFi networks
WiFiMulti wifiMulti;
WiFiUDP ntpUDP;
IPAddress myip;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -5 * 60 * 60);
bool gEnabled = false;

AsyncWebServer server(HTTP_PORT);
AsyncWebSocket ws("/ws");

JsonDocument jsonDocRx;

// IO Devices
Reservoir reservoir(&timeClient);
Pump pumps[4] = {Pump(PIN_PUMP_MID_ENABLE, PUMP_MID, &timeClient),
                 Pump(PIN_PUMP_BOTTOM_ENABLE, PUMP_BOTTOM, &timeClient),
                 Pump(PIN_PUMP_TOP_ENABLE, PUMP_TOP, &timeClient),
                 Pump(PIN_PUMP_4_ENABLE, PUMP_4, &timeClient)};
Tray trays[4] = {Tray(PIN_MOISTURE_SENSOR_MID, TRAY_MID, &timeClient),
                 Tray(PIN_MOISTURE_SENSOR_BOTTOM, TRAY_BOTTOM, &timeClient),
                 Tray(PIN_MOISTURE_SENSOR_TOP, TRAY_TOP, &timeClient),
                 Tray(PIN_MOISTURE_SENSOR_4, TRAY_4, &timeClient)};
std::array<IODevice *, 9> ioDevices{&reservoir, &pumps[0], &pumps[1], &pumps[2], &pumps[3], &trays[0], &trays[1], &trays[2], &trays[3]};
JsonData jsonData[2];
JsonData *lastJsonData = &jsonData[0];
JsonData *currJsonData = &jsonData[1];

extern const char index_html_start[] asm("_binary_src_index_html_start");
const String html = String((const char *)index_html_start);

bool wsconnected = false;

void notFound(AsyncWebServerRequest *request)
{
    request->send(404, "text/plain", "Not found");
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    if (type == WS_EVT_CONNECT)
    {
        wsconnected = true;
        LOG("ws[%s][%u] connect\n", server->url(), client->id());
        // client->printf("Hello Client %u :)", client->id());
        client->ping();
    }
    else if (type == WS_EVT_DISCONNECT)
    {
        wsconnected = false;
        LOG("ws[%s][%u] disconnect\n", server->url(), client->id());
    }
    else if (type == WS_EVT_ERROR)
    {
        LOG("ws[%s][%u] error(%u): %s\n", server->url(), client->id(),
            *((uint16_t *)arg), (char *)data);
    }
    else if (type == WS_EVT_PONG)
    {
        LOG("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len,
            (len) ? (char *)data : "");
    }
    else if (type == WS_EVT_DATA)
    {
        AwsFrameInfo *info = (AwsFrameInfo *)arg;
        String msg = "";
        if (info->final && info->index == 0 && info->len == len)
        {
            // the whole message is in a single frame and we got all of it's data
            LOG("ws[%s][%u] %s-msg[%llu]\r\n", server->url(), client->id(),
                (info->opcode == WS_TEXT) ? "txt" : "bin", info->len);

            if (info->opcode == WS_TEXT)
            {
                for (size_t i = 0; i < info->len; i++)
                {
                    msg += (char)data[i];
                }
                LOG("%s\r\n\r\n", msg.c_str());

                deserializeJson(jsonDocRx, msg);
                String secret = jsonDocRx["secret"];

                LOG("Secret: %s\n", secret.c_str());
                if (secret == SECRET_KEY)
                {
                    if (jsonDocRx.containsKey("togglePump"))
                    {
                        LOG("Toggle Pump\n");
                        uint8_t pump = jsonDocRx["togglePump"];
                        uint8_t duty = jsonDocRx["duty"];
                        if (pump > 0 && pump <= 4)
                        {
                            if (pumps[pump - 1].duty() == 0)
                            {
                                pumps[pump - 1].setDuty(duty);
                            }
                            else
                            {
                                pumps[pump - 1].setDuty(0);
                            }
                        }
                    }
                    else if (jsonDocRx.containsKey("setEnabled"))
                    {
                        bool enabled = jsonDocRx["setEnabled"];
                        LOG("Setting global enable to %d\n", enabled);
                        gEnabled = enabled;
                        EEPROM.write(0, gEnabled);
                        EEPROM.commit();
                        for (auto &pump : pumps)
                        {
                            pump.setEnabled(gEnabled);
                        }
                    }
                }
                else
                {
                    LOG("Secret key mismatch\n");
                }

                jsonDocRx.clear();
            }
        }
    }
}

void taskWifi(void *parameter);
void taskIoTPlotter(void *parameter);
void taskData(void *parameter);
void taskManager(void *parameter);

void setup()
{
    Serial.begin(115200);
    LOG("Enabling WDT\n");
    esp_task_wdt_init(WDT_TIMEOUT, true); // enable panic so ESP32 restarts

    EEPROM.begin(EEPROM_SIZE);
    gEnabled = EEPROM.read(0);
    LOG("EEPROM read global enable: %d\n", gEnabled);
    for (auto &pump : pumps)
    {
        pump.setEnabled(gEnabled);
    }

    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);

    for (auto &ioDevice : ioDevices)
    {
        ioDevice->begin();
    }

    TaskHandle_t wifiHandle = nullptr;
    xTaskCreate(taskWifi,         /* Task function. */
                "taskWifi",       /* String with name of task. */
                20000,            /* Stack size in bytes. */
                NULL,             /* Parameter passed as input of the task */
                4,                /* Priority of the task. */
                &wifiHandle);     /* Task handle. */
    esp_task_wdt_add(wifiHandle); // add the task to the WDT watch

    TaskHandle_t iotPlotterHandle = nullptr;
    xTaskCreate(taskIoTPlotter,         /* Task function. */
                "taskIoTPlotter",       /* String with name of task. */
                20000,                  /* Stack size in bytes. */
                NULL,                   /* Parameter passed as input of the task */
                5,                      /* Priority of the task. */
                &iotPlotterHandle);     /* Task handle. */
    esp_task_wdt_add(iotPlotterHandle); // add the task to the WDT watch

    TaskHandle_t dataHandle = nullptr;
    auto data = xTaskCreate(taskData,     /* Task function. */
                            "taskData",   /* String with name of task. */
                            5000,         /* Stack size in bytes. */
                            NULL,         /* Parameter passed as input of the task */
                            3,            /* Priority of the task. */
                            &dataHandle); /* Task handle. */
    esp_task_wdt_add(dataHandle);         // add the task to the WDT watch

    TaskHandle_t managerHandle = nullptr;
    xTaskCreate(taskManager,         /* Task function. */
                "taskManager",       /* String with name of task. */
                5000,                /* Stack size in bytes. */
                NULL,                /* Parameter passed as input of the task */
                2,                   /* Priority of the task. */
                &managerHandle);     /* Task handle. */
    esp_task_wdt_add(managerHandle); // add the task to the WDT watch
}

void taskManager(void *parameter)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    int lastDay = 0;
    int lastHour = 0;
    int lastMinute = 0;
    int lastSeconds = 0;
    while (1)
    {
        // We will water on Sunday(0) and Wednesday(3)
        // Watering will start at 9:00 AM

        digitalWrite(PIN_LED, 1);
        int seconds = timeClient.getSeconds();
        int minutes = timeClient.getMinutes();
        int hours = timeClient.getHours();
        int day = timeClient.getDay();

        if (seconds != lastSeconds)
        {
            // New Second
            lastSeconds = seconds;

            if (minutes != lastMinute)
            {
                // New Minute
                LOG("Time: %02d:%02d:%02d\r\n", hours, minutes, seconds);
                lastMinute = minutes;

                if (day == 0 || day == 3)
                {
                    if (hours == 9 && minutes == 0)
                    {
                        pumps[PUMP_TOP].runPump(2 * 60, 220);
                        pumps[PUMP_MID].runPump(2 * 60, 220);
                        pumps[PUMP_BOTTOM].runPump(2 * 60, 220);
                    }
                }

                if (hours != lastHour)
                {
                    lastHour = hours;

                    if (day != lastDay)
                    {
                        // New Day
                        LOG("It's a new day!: %d\r\n", day);
                        lastDay = day;
                    }
                }
            }
        }

        for (auto &ioDevice : ioDevices)
        {
            ioDevice->update();
        }

        digitalWrite(PIN_LED, 0);
        esp_task_wdt_reset();
        vTaskDelayUntil(&xLastWakeTime, 100 * portTICK_PERIOD_MS);
    }
}

void postIoTPlotterData(const char *url, const char *apiKey, const String &data)
{
    LOG("POST to %s - %s\n", url, data.c_str());

    HTTPClient http;
    http.begin(url);
    http.addHeader("Connection", "Close");
    http.addHeader("Content-Type", "application/json");
    http.addHeader("api-key", apiKey);
    int httpCode = http.POST(data);
    if (httpCode > 0)
    {
        String payload = http.getString();
        LOG("HTTP code: %d\n", httpCode);
        LOG("Response: %s\n", payload.c_str());
    }
    else
    {
        LOG("Error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
}

void taskIoTPlotter(void *parameter)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    int lastMinute = 0;
    int lastSeconds = 0;
    uint8_t lastPumpDuties[PUMP_MAX] = {0, 0, 0, 0};
    String output;
    while (1)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            bool logData = false;
            int seconds = timeClient.getSeconds();
            int minutes = timeClient.getMinutes();

            if (seconds != lastSeconds)
            {
                // New Second
                lastSeconds = seconds;

                if (minutes != lastMinute)
                {
                    // New Minute
                    lastMinute = minutes;
                    if (minutes % IOTPLOTTER_LOG_INTERVAL_M == 0)
                    {
                        logData = true;
                    }
                }
                else if (currJsonData->pumps[PUMP_TOP].duty != lastPumpDuties[PUMP_TOP] ||
                         currJsonData->pumps[PUMP_MID].duty != lastPumpDuties[PUMP_MID] ||
                         currJsonData->pumps[PUMP_BOTTOM].duty != lastPumpDuties[PUMP_BOTTOM] ||
                         currJsonData->pumps[PUMP_4].duty != lastPumpDuties[PUMP_4])
                {
                    // Pump duty changed
                    lastPumpDuties[PUMP_TOP] = currJsonData->pumps[PUMP_TOP].duty;
                    lastPumpDuties[PUMP_MID] = currJsonData->pumps[PUMP_MID].duty;
                    lastPumpDuties[PUMP_BOTTOM] = currJsonData->pumps[PUMP_BOTTOM].duty;
                    lastPumpDuties[PUMP_4] = currJsonData->pumps[PUMP_4].duty;
                    logData = true;
                }
            }

            if (logData)
            {
                currJsonData->serializeIotPlotter(output);
                // post to iotplotter
                postIoTPlotterData(IOTPLOTTER_URL, IOTPLOTTER_API_KEY, output);
            }
        }

        esp_task_wdt_reset();
        vTaskDelayUntil(&xLastWakeTime, 1000 * portTICK_PERIOD_MS);
    }
}

void taskData(void *parameter)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while (1)
    {
        for (auto &ioDevice : ioDevices)
        {
            ioDevice->fillData(*currJsonData);
        }
        esp_task_wdt_reset();
        vTaskDelayUntil(&xLastWakeTime, 1000 * portTICK_PERIOD_MS);
    }
}

void taskWifi(void *parameter)
{
    uint8_t stat = WL_DISCONNECTED;
    String output;
    int cnt = 0;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    /* Configure Wi-Fi */
    for (int i = 0; i < NUM_NETWORKS; i++)
    {
        wifiMulti.addAP(ssidTab[i], passwordTab[i]);
        Serial.printf("WiFi %d: SSID: \"%s\" ; PASS: \"%s\"\r\n", i, ssidTab[i],
                      passwordTab[i]);
    }

    while (stat != WL_CONNECTED)
    {
        stat = wifiMulti.run();
        Serial.printf("WiFi status: %d\r\n", (int)stat);
        esp_task_wdt_reset();
        vTaskDelayUntil(&xLastWakeTime, 100 * portTICK_PERIOD_MS);
    }

    Serial.printf("WiFi connected\r\n");
    Serial.printf("IP address: ");
    Serial.println(myip = WiFi.localIP());

    /* Start NTP */
    timeClient.begin();

    /* Start web server and web socket server */
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/html", html); });
    server.onNotFound(notFound);
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);
    server.begin();

    LOG("Type in browser:\r\n%s:%d\r\n", hostName, HTTP_PORT);

    while (1)
    {
        while (WiFi.status() == WL_CONNECTED)
        {
            timeClient.update();
            if ((wsconnected == true) &&
                (*lastJsonData != *currJsonData))
            {
                currJsonData->serializeWebServer(gEnabled, timeClient.getFormattedTime(), output);
                *lastJsonData = *currJsonData;

                Serial.printf("%s\n", output.c_str());
                if (ws.availableForWriteAll())
                {
                    ws.textAll(output);
                }
                else
                {
                    Serial.printf("...queue is full\r\n");
                }
            }
            esp_task_wdt_reset();
            vTaskDelayUntil(&xLastWakeTime, 1000 * portTICK_PERIOD_MS);
        }

        stat = wifiMulti.run();
        myip = WiFi.localIP();
        LOG("WiFi status: %d\r\n", (int)stat);
        esp_task_wdt_reset();
        vTaskDelayUntil(&xLastWakeTime, 500 * portTICK_PERIOD_MS);
    }
}

void loop()
{
    Serial.printf("[RAM: %d]\r\n", esp_get_free_heap_size());
    delay(10000);
    // add ping mechanism in case of websocket connection timeout
    // ws.cleanupClients();
    // ws.pingAll();
}
