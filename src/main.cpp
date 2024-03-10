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



// Load Wi-Fi library
#include <WiFi.h>
#include "pins.h"
#include "pwm.h"

// Replace with your network credentials
const char *ssid = "tinkle_IoT";
const char *password = "tinkle_key";

Pwm pwm;

// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;

// Auxiliar variables to store the current output state
String motorAState = "off";
String motorBState = "off";

// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0;
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;

void setup()
{
  Serial.begin(9600);

  pwm.begin();

  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
}

void loop()
{
  WiFiClient client = server.available(); // Listen for incoming clients

  if (client)
  { // If a new client connects,
    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client."); // print a message out in the serial port
    String currentLine = "";       // make a String to hold incoming data from the client
    while (client.connected() && currentTime - previousTime <= timeoutTime)
    { // loop while the client's connected
      currentTime = millis();
      if (client.available())
      {                         // if there's bytes to read from the client,
        char c = client.read(); // read a byte, then
        Serial.write(c);        // print it out the serial monitor
        header += c;
        if (c == '\n')
        { // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0)
          {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            // turns the GPIOs on and off
            if (header.indexOf("GET /motor_a/on") >= 0)
            {
              Serial.println("motor A on");
              motorAState = "on";
              // digitalWrite(PIN_MOTOR_A_ENABLE, HIGH);
              pwm.setDuty(Pwm::MOTOR_A, 512);
            }
            else if (header.indexOf("GET /motor_a/off") >= 0)
            {
              Serial.println("motor A off");
              motorAState = "off";
              pwm.setDuty(Pwm::MOTOR_A, 0);
              // digitalWrite(PIN_MOTOR_A_ENABLE, LOW);
            }
            else if (header.indexOf("GET /motor_b/on") >= 0)
            {
              Serial.println("motor B on");
              motorBState = "on";
              // digitalWrite(PIN_MOTOR_B_ENABLE, HIGH);
              pwm.setDuty(Pwm::MOTOR_B, 512);
            }
            else if (header.indexOf("GET /motor_b/off") >= 0)
            {
              Serial.println("motor B off");
              motorBState = "off";
              // digitalWrite(PIN_MOTOR_B_ENABLE, LOW);
              pwm.setDuty(Pwm::MOTOR_B, 0);
            }

            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #555555;}</style></head>");

            // Web Page Heading
            client.println("<body><h1>ESP32 Web Server</h1>");

            // Display current state, and ON/OFF buttons for Motor A
            client.println("<p>Motor A - State " + motorAState + "</p>");
            // If the motorAState is off, it displays the ON button
            if (motorAState == "off")
            {
              client.println("<p><a href=\"/motor_a/on\"><button class=\"button\">ON</button></a></p>");
            }
            else
            {
              client.println("<p><a href=\"/motor_a/off\"><button class=\"button button2\">OFF</button></a></p>");
            }

            // Display current state, and ON/OFF buttons for Motor B
            client.println("<p>Motor B - State " + motorBState + "</p>");
            // If the motorBState is off, it displays the ON button
            if (motorBState == "off")
            {
              client.println("<p><a href=\"/motor_b/on\"><button class=\"button\">ON</button></a></p>");
            }
            else
            {
              client.println("<p><a href=\"/motor_b/off\"><button class=\"button button2\">OFF</button></a></p>");
            }
            client.println("</body></html>");

            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          }
          else
          { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        }
        else if (c != '\r')
        {                   // if you got anything else but a carriage return character,
          currentLine += c; // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}