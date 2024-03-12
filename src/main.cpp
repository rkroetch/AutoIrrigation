#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <EasyDDNS.h>
#include <ESPAsyncWebServer.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiMulti.h>

/* =============== config section start =============== */

#define HTTP_PORT 8080

const int BUTTON_PIN = 35;
const int LED_PIN = 2;

const char* hostName = "robowater.duckdns.org";

#if __has_include("credentials.h")
#include "credentials.h"
#else
// WiFi credentials
#define NUM_NETWORKS 1
// Add your networks credentials here
const char* ssidTab[NUM_NETWORKS] = {
    "tinkle_IoT"
};
const char* passwordTab[NUM_NETWORKS] = {
    "tinkle_key"
};
#endif
/* =============== config section end =============== */

#define LOG(f_, ...) \
  { Serial.printf((f_), ##__VA_ARGS__); }

// you can provide credentials to multiple WiFi networks
WiFiMulti wifiMulti;
IPAddress myip;

// https://github.com/me-no-dev/ESPAsyncWebServer/issues/324 - sometimes
AsyncWebServer server(HTTP_PORT);
AsyncWebSocket ws("/ws");

StaticJsonDocument<200> jsonDocTx;
StaticJsonDocument<100> jsonDocRx;

extern const char index_html_start[] asm("_binary_src_index_html_start");
const String html = String((const char*)index_html_start);

bool wsconnected = false;

void notFound(AsyncWebServerRequest* request) {
  request->send(404, "text/plain", "Not found");
}

void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
               AwsEventType type, void* arg, uint8_t* data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    wsconnected = true;
    LOG("ws[%s][%u] connect\n", server->url(), client->id());
    // client->printf("Hello Client %u :)", client->id());
    client->ping();
  } else if (type == WS_EVT_DISCONNECT) {
    wsconnected = false;
    LOG("ws[%s][%u] disconnect\n", server->url(), client->id());
  } else if (type == WS_EVT_ERROR) {
    LOG("ws[%s][%u] error(%u): %s\n", server->url(), client->id(),
        *((uint16_t*)arg), (char*)data);
  } else if (type == WS_EVT_PONG) {
    LOG("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len,
        (len) ? (char*)data : "");
  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo* info = (AwsFrameInfo*)arg;
    String msg = "";
    if (info->final && info->index == 0 && info->len == len) {
      // the whole message is in a single frame and we got all of it's data
      LOG("ws[%s][%u] %s-msg[%llu]\r\n", server->url(), client->id(),
          (info->opcode == WS_TEXT) ? "txt" : "bin", info->len);

      if (info->opcode == WS_TEXT) {
        for (size_t i = 0; i < info->len; i++) {
          msg += (char)data[i];
        }
        LOG("%s\r\n\r\n", msg.c_str());

        deserializeJson(jsonDocRx, msg);

        uint8_t ledState = jsonDocRx["led"];
        if (ledState == 1) {
          digitalWrite(LED_PIN, HIGH);
        }
        if (ledState == 0) {
          digitalWrite(LED_PIN, LOW);
        }
        jsonDocRx.clear();
      }
    }
  }
}

void taskWifi(void* parameter);
void taskStatus(void* parameter);

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  xTaskCreate(taskWifi,   /* Task function. */
              "taskWifi", /* String with name of task. */
              20000,      /* Stack size in bytes. */
              NULL,       /* Parameter passed as input of the task */
              2,          /* Priority of the task. */
              NULL);      /* Task handle. */
}

void taskWifi(void* parameter) {
  uint8_t stat = WL_DISCONNECTED;
  static char output[200];
  int cnt = 0;
  int lastButtonState = digitalRead(BUTTON_PIN);
  TickType_t xLastWakeTime = xTaskGetTickCount();

  /* Configure Wi-Fi */
  for (int i = 0; i < NUM_NETWORKS; i++) {
    wifiMulti.addAP(ssidTab[i], passwordTab[i]);
    Serial.printf("WiFi %d: SSID: \"%s\" ; PASS: \"%s\"\r\n", i, ssidTab[i],
                  passwordTab[i]);
  }

  while (stat != WL_CONNECTED) {
    stat = wifiMulti.run();
    Serial.printf("WiFi status: %d\r\n", (int)stat);
    delay(100);
  }

  Serial.printf("WiFi connected\r\n");
  Serial.printf("IP address: ");
  Serial.println(myip = WiFi.localIP());

  EasyDDNS.service("duckdns");
  EasyDDNS.client("robowater.duckdns.org", "db847a14-8e21-42c7-929e-fa20bb1f8af5"); // Enter your DDNS Domain & Token

  // Get Notified when your IP changes
  EasyDDNS.onUpdate([&](const char* oldIP, const char* newIP){
    Serial.print("EasyDDNS - IP Change Detected: ");
    Serial.println(newIP);
  });

  /* Start web server and web socket server */
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/html", html);
  });
  server.onNotFound(notFound);
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.begin();

  LOG("Type in browser:\r\n%s:%d\r\n", hostName, HTTP_PORT);

  while (1) {
    while (WiFi.status() == WL_CONNECTED) {
      cnt++;
      if ((wsconnected == true) &&
          ((lastButtonState != digitalRead(BUTTON_PIN)) || (cnt % 100 == 0))) {
        lastButtonState = digitalRead(BUTTON_PIN);
        jsonDocTx.clear();
        jsonDocTx["counter"] = cnt;
        jsonDocTx["button"] = lastButtonState;

        serializeJson(jsonDocTx, output, 200);

        Serial.printf("Sending: %s", output);
        if (ws.availableForWriteAll()) {
          ws.textAll(output);
          Serial.printf("...done\r\n");
        } else {
          Serial.printf("...queue is full\r\n");
        }
      } else {
        vTaskDelayUntil(&xLastWakeTime, 10 / portTICK_PERIOD_MS);
      }
    }

    stat = wifiMulti.run();
    myip = WiFi.localIP();
    LOG("WiFi status: %d\r\n", (int)stat);
    delay(500);
  }
}

void loop() {
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