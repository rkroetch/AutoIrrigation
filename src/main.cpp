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
Pump pump_1(PIN_PUMP_1_ENABLE, PIN_PUMP_1_PHASE, PUMP_1, &timeClient);
Pump pump_2(PIN_PUMP_2_ENABLE, PIN_PUMP_2_PHASE, PUMP_2, &timeClient);
Tray tray_1(PIN_MOISTURE_SENSOR_1, TRAY_1, &timeClient);
Tray tray_2(PIN_MOISTURE_SENSOR_2, TRAY_2, &timeClient);
Tray tray_3(PIN_MOISTURE_SENSOR_3, TRAY_3, &timeClient);
Tray tray_4(PIN_MOISTURE_SENSOR_4, TRAY_4, &timeClient);
std::array<IODevice *, 3> ioDevices{&reservoir, &pump_1, &pump_2};
JSONData jsonData[2];
JSONData *lastJsonData = &jsonData[0];
JSONData *currJsonData = &jsonData[1];

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

                uint8_t ledState = jsonDocRx["led"];
                if (ledState == 1)
                {
                    digitalWrite(PIN_LED, HIGH);
                }
                if (ledState == 0)
                {
                    digitalWrite(PIN_LED, LOW);
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
        if (timeClient.getSeconds() == 0)
        {
            LOG("Time: %s\n", timeClient.getFormattedTime());
            pump_1.runPump(10, 128);
            pump_2.runPump(10, 128);
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
    static char output[200];
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
                jsonDocTx["pump1AccTime"] = currJsonData->pumps[0].accumulatedTime;
                jsonDocTx["pump1Duty"] = currJsonData->pumps[0].duty;
                jsonDocTx["pump2AccTime"] = currJsonData->pumps[1].accumulatedTime;
                jsonDocTx["pump2Duty"] = currJsonData->pumps[1].duty;
                jsonDocTx["tray1Level"] = currJsonData->trays[0].moisture;
                jsonDocTx["tray2Level"] = currJsonData->trays[1].moisture;
                jsonDocTx["tray3Level"] = currJsonData->trays[2].moisture;
                jsonDocTx["tray4Level"] = currJsonData->trays[3].moisture;
                jsonDocTx["lastUpdate"] = timeClient.getFormattedTime();

                serializeJson(jsonDocTx, output, 200);
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

// #include <Arduino.h>

// // NeoPixel Ring simple sketch (c) 2013 Shae Erisson
// // Released under the GPLv3 license to match the rest of the
// // Adafruit NeoPixel library

// #include <Adafruit_NeoPixel.h>
// #ifdef __AVR__
//  #include <avr/power.h> // Required for 16 MHz Adafruit Trinket
// #endif

// // Which pin on the Arduino is connected to the NeoPixels?
// #define PIN        12 // On Trinket or Gemma, suggest changing this to 1

// // How many NeoPixels are attached to the Arduino?
// #define NUMPIXELS 12 // Popular NeoPixel ring size

// // When setting up the NeoPixel library, we tell it how many pixels,
// // and which pin to use to send signals. Note that for older NeoPixel
// // strips you might need to change the third parameter -- see the
// // strandtest example for more information on possible values.
// Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// #define DELAYVAL 500 // Time (in milliseconds) to pause between pixels

// void setup() {
//   // These lines are specifically to support the Adafruit Trinket 5V 16 MHz.
//   // Any other board, you can remove this part (but no harm leaving it):
// #if defined(__AVR_ATtiny85__) && (F_CPU == 16000000)
//   clock_prescale_set(clock_div_1);
// #endif
//   // END of Trinket-specific code.

//   pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
// }

// void loop() {
//   pixels.clear(); // Set all pixel colors to 'off'

//   // The first NeoPixel in a strand is #0, second is 1, all the way up
//   // to the count of pixels minus one.
//   for(int i=0; i<NUMPIXELS; i++) { // For each pixel...

//     // pixels.Color() takes RGB values, from 0,0,0 up to 255,255,255
//     // Here we're using a moderately bright green color:
//     pixels.setPixelColor(i, pixels.Color(0, 10, 0));

//     pixels.show();   // Send the updated pixel colors to the hardware.

//     delay(DELAYVAL); // Pause before next pass through loop
//   }
// }

// // Load Wi-Fi library
// #include <WiFi.h>
// #include "pins.h"
// #include "pump.h"

// // Replace with your network credentials
// const char *ssid = "tinkle_IoT";
// const char *password = "tinkle_key";

// Pump pwm;

// // Set web server port number to 80
// WiFiServer server(80);

// // Variable to store the HTTP request
// String header;

// // Auxiliar variables to store the current output state
// String motorAState = "off";
// String motorBState = "off";

// // Current time
// unsigned long currentTime = millis();
// // Previous time
// unsigned long previousTime = 0;
// // Define timeout time in milliseconds (example: 2000ms = 2s)
// const long timeoutTime = 2000;

// void setup()
// {
//   Serial.begin(9600);

//   pwm.begin();

//   // Connect to Wi-Fi network with SSID and password
//   Serial.print("Connecting to ");
//   Serial.println(ssid);
//   WiFi.begin(ssid, password);
//   while (WiFi.status() != WL_CONNECTED)
//   {
//     delay(500);
//     Serial.print(".");
//   }
//   // Print local IP address and start web server
//   Serial.println("");
//   Serial.println("WiFi connected.");
//   Serial.println("IP address: ");
//   Serial.println(WiFi.localIP());
//   server.begin();
// }

// void loop()
// {
//   WiFiClient client = server.available(); // Listen for incoming clients

//   if (client)
//   { // If a new client connects,
//     currentTime = millis();
//     previousTime = currentTime;
//     Serial.println("New Client."); // print a message out in the serial port
//     String currentLine = "";       // make a String to hold incoming data from the client
//     while (client.connected() && currentTime - previousTime <= timeoutTime)
//     { // loop while the client's connected
//       currentTime = millis();
//       if (client.available())
//       {                         // if there's bytes to read from the client,
//         char c = client.read(); // read a byte, then
//         Serial.write(c);        // print it out the serial monitor
//         header += c;
//         if (c == '\n')
//         { // if the byte is a newline character
//           // if the current line is blank, you got two newline characters in a row.
//           // that's the end of the client HTTP request, so send a response:
//           if (currentLine.length() == 0)
//           {
//             // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
//             // and a content-type so the client knows what's coming, then a blank line:
//             client.println("HTTP/1.1 200 OK");
//             client.println("Content-type:text/html");
//             client.println("Connection: close");
//             client.println();

//             // turns the GPIOs on and off
//             if (header.indexOf("GET /motor_a/on") >= 0)
//             {
//               Serial.println("motor A on");
//               motorAState = "on";
//               // digitalWrite(PIN_PUMP_1_ENABLE, HIGH);
//               pwm.setDuty(Pump::PUMP_1, 128);
//             }
//             else if (header.indexOf("GET /motor_a/off") >= 0)
//             {
//               Serial.println("motor A off");
//               motorAState = "off";
//               pwm.setDuty(Pump::PUMP_1, 0);
//               // digitalWrite(PIN_PUMP_1_ENABLE, LOW);
//             }
//             else if (header.indexOf("GET /motor_b/on") >= 0)
//             {
//               Serial.println("motor B on");
//               motorBState = "on";
//               // digitalWrite(PIN_PUMP_2_ENABLE, HIGH);
//               pwm.setDuty(Pump::PUMP_2, 128);
//             }
//             else if (header.indexOf("GET /motor_b/off") >= 0)
//             {
//               Serial.println("motor B off");
//               motorBState = "off";
//               // digitalWrite(PIN_PUMP_2_ENABLE, LOW);
//               pwm.setDuty(Pump::PUMP_2, 0);
//             }

//             // Display the HTML web page
//             client.println("<!DOCTYPE html><html>");
//             client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
//             client.println("<link rel=\"icon\" href=\"data:,\">");
//             // CSS to style the on/off buttons
//             // Feel free to change the background-color and font-size attributes to fit your preferences
//             client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
//             client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
//             client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
//             client.println(".button2 {background-color: #555555;}</style></head>");

//             // Web Page Heading
//             client.println("<body><h1>ESP32 Web Server</h1>");

//             // Display current state, and ON/OFF buttons for Motor A
//             client.println("<p>Motor A - State " + motorAState + "</p>");
//             // If the motorAState is off, it displays the ON button
//             if (motorAState == "off")
//             {
//               client.println("<p><a href=\"/motor_a/on\"><button class=\"button\">ON</button></a></p>");
//             }
//             else
//             {
//               client.println("<p><a href=\"/motor_a/off\"><button class=\"button button2\">OFF</button></a></p>");
//             }

//             // Display current state, and ON/OFF buttons for Motor B
//             client.println("<p>Motor B - State " + motorBState + "</p>");
//             // If the motorBState is off, it displays the ON button
//             if (motorBState == "off")
//             {
//               client.println("<p><a href=\"/motor_b/on\"><button class=\"button\">ON</button></a></p>");
//             }
//             else
//             {
//               client.println("<p><a href=\"/motor_b/off\"><button class=\"button button2\">OFF</button></a></p>");
//             }
//             client.println("</body></html>");

//             // The HTTP response ends with another blank line
//             client.println();
//             // Break out of the while loop
//             break;
//           }
//           else
//           { // if you got a newline, then clear currentLine
//             currentLine = "";
//           }
//         }
//         else if (c != '\r')
//         {                   // if you got anything else but a carriage return character,
//           currentLine += c; // add it to the end of the currentLine
//         }
//       }
//     }
//     // Clear the header variable
//     header = "";
//     // Close the connection
//     client.stop();
//     Serial.println("Client disconnected.");
//     Serial.println("");
//   }
// }