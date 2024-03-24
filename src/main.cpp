#include <array>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
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

constexpr const char *hostName = "robowater.duckdns.org";

#if __has_include("credentials.h")
#include "credentials.h"
#else
// WiFi credentials
#define NUM_NETWORKS 1
// Add your networks credentials here
const char *ssidTab[NUM_NETWORKS] = {
    "tinkle_IoT"};
const char *passwordTab[NUM_NETWORKS] = {
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

AsyncWebServer server(HTTP_PORT);
AsyncWebSocket ws("/ws");

JsonDocument jsonDocTx;
JsonDocument jsonDocRx;

// IO Devices
Reservoir reservoir(&timeClient);
Pump pumps[4] = {Pump(PIN_PUMP_1_ENABLE, PUMP_1, &timeClient),
                 Pump(PIN_PUMP_2_ENABLE, PUMP_2, &timeClient),
                 Pump(PIN_PUMP_3_ENABLE, PUMP_3, &timeClient),
                 Pump(PIN_PUMP_4_ENABLE, PUMP_4, &timeClient)};
Tray trays[4] = {Tray(PIN_MOISTURE_SENSOR_1, TRAY_1, &timeClient),
                    Tray(PIN_MOISTURE_SENSOR_2, TRAY_2, &timeClient),
                    Tray(PIN_MOISTURE_SENSOR_3, TRAY_3, &timeClient),
                    Tray(PIN_MOISTURE_SENSOR_4, TRAY_4, &timeClient)};
std::array<IODevice *, 9> ioDevices{&reservoir, &pumps[0], &pumps[1], &pumps[2], &pumps[3], &trays[0], &trays[1], &trays[2], &trays[3]};
JSONData jsonData[2];
JSONData *lastJsonData = &jsonData[0];
JSONData *currJsonData = &jsonData[1];

uint16_t pTTs[4] = {0};
uint16_t pSTs[4] = {0};

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

                uint8_t pump = jsonDocRx["togglePump"];
                uint8_t duty = jsonDocRx["duty"];
                if (pump > 0 && pump <= 4)
                {
                    if (pumps[pump - 1].duty() == 0)
                    {
                        pSTs[pump - 1] = millis();
                        pumps[pump - 1].setDuty(duty);
                    }
                    else
                    {
                        pTTs[pump - 1] = millis() - pSTs[pump - 1];
                        pumps[pump - 1].setDuty(0);
                    }
                }

                jsonDocRx.clear();
            }
        }
    }
}

void taskWifi(void *parameter);
void taskData(void *parameter);
void taskManager(void *parameter);

void setup()
{
    Serial.begin(115200);
    LOG("Enabling WDT\n");
    esp_task_wdt_init(WDT_TIMEOUT, true); // enable panic so ESP32 restarts

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
    while (1)
    {
        digitalWrite(PIN_LED, 1);
        // if (timeClient.getSeconds() == 0 && pump_1.duty() == 0)
        // {
        //     LOG("%s - Starting Pump 1\n", timeClient.getFormattedTime());
        //     pump_1.runPump(10, 200);
        // }
        // if (timeClient.getSeconds() == 10 && pump_2.duty() == 0)
        // {
        //     LOG("%s - Starting Pump 2\n", timeClient.getFormattedTime());
        //     pump_2.runPump(10, 200);
        // }
        // if (timeClient.getSeconds() == 20 && pump_3.duty() == 0)
        // {
        //     LOG("%s - Starting Pump 3\n", timeClient.getFormattedTime());
        //     pump_3.runPump(10, 200);
        // }
        // if (timeClient.getSeconds() == 30 && pump_4.duty() == 0)
        // {
        //     LOG("%s - Starting Pump 4\n", timeClient.getFormattedTime());
        //     pump_4.runPump(10, 200);
        // }

        for (auto &ioDevice : ioDevices)
        {
            ioDevice->update();
        }

        digitalWrite(PIN_LED, 0);
        esp_task_wdt_reset();
        vTaskDelayUntil(&xLastWakeTime, 100 * portTICK_PERIOD_MS);
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
    constexpr uint16_t JSON_BUFFER_SIZE = 1024;
    static char output[JSON_BUFFER_SIZE];
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
                jsonDocTx.clear();
                jsonDocTx["rLevel"] = currJsonData->reservoir.level;
                jsonDocTx["rRaw"] = currJsonData->reservoir.rawData;
                jsonDocTx["p1AccTime"] = currJsonData->pumps[0].accumulatedTime;
                jsonDocTx["p1Duty"] = currJsonData->pumps[0].duty;
                jsonDocTx["p2AccTime"] = currJsonData->pumps[1].accumulatedTime;
                jsonDocTx["p2Duty"] = currJsonData->pumps[1].duty;
                jsonDocTx["p3AccTime"] = currJsonData->pumps[2].accumulatedTime;
                jsonDocTx["p3Duty"] = currJsonData->pumps[2].duty;
                jsonDocTx["p4AccTime"] = currJsonData->pumps[3].accumulatedTime;
                jsonDocTx["p4Duty"] = currJsonData->pumps[3].duty;
                jsonDocTx["t1Moisture"] = currJsonData->trays[0].moisture;
                jsonDocTx["t2Moisture"] = currJsonData->trays[1].moisture;
                jsonDocTx["t3Moisture"] = currJsonData->trays[2].moisture;
                jsonDocTx["t4Moisture"] = currJsonData->trays[3].moisture;
                jsonDocTx["p1TT"] = pTTs[0];
                jsonDocTx["p2TT"] = pTTs[1];
                jsonDocTx["p3TT"] = pTTs[2];
                jsonDocTx["p4TT"] = pTTs[3];
                jsonDocTx["lastUpdate"] = timeClient.getFormattedTime();

                serializeJson(jsonDocTx, output, JSON_BUFFER_SIZE);
                *lastJsonData = *currJsonData;

                Serial.printf("%s\n", output);
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
