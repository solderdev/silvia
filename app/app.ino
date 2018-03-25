/**
 * board: NodeMCU-32S
 * flash freq: 80MHz
 * upload speed: 921600 b
 */
#include <WiFi.h>
#include "private_defines.h"

//int LED_BUILTIN = 2;

uint32_t lastmilli = 0;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  delay(100);

  setup_wifi();
  setup_sensors();
  setup_pid(20.0, 1.5, 5, 1000);
  
  lastmilli = millis();
}

void loop() {

  if (millis() - lastmilli > 500 )
  {
    lastmilli = millis();

//    Serial.println(WiFi.RSSI());
  }
  
  update_sensors();
  service_server();
  service_display();
  service_pid();

  yield();
}
 

//uint32_t lastmilli = 0;
//void loop() {
//  if (millis() - 500 >= lastmilli)
//  {
//    lastmilli = millis();
//    int sensorValue = analogRead(A0);
//    //Serial.println(sensorValue);
//    Serial.println(WiFi.RSSI());
//  }
//
//  WiFiClient client = server.available();   // listen for incoming clients
//
//  if (client) {                             // if you get a client,
//    Serial.println("New Client.");           // print a message out the serial port
//    String currentLine = "";                // make a String to hold incoming data from the client
//    while (client.connected()) {            // loop while the client's connected
//      if (client.available()) {             // if there's bytes to read from the client,
//        char c = client.read();             // read a byte, then
//        Serial.write(c);                    // print it out the serial monitor
//        if (c == '\n') {                    // if the byte is a newline character
//
//          // if the current line is blank, you got two newline characters in a row.
//          // that's the end of the client HTTP request, so send a response:
//          if (currentLine.length() == 0) {
//            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
//            // and a content-type so the client knows what's coming, then a blank line:
//            client.println("HTTP/1.1 200 OK");
//            client.println("Content-type:text/html");
//            client.println();
//
//            // the content of the HTTP response follows the header:
//            client.print("Click <a href=\"/H\">here</a> to turn the LED on pin 5 on.<br>");
//            client.print("Click <a href=\"/L\">here</a> to turn the LED on pin 5 off.<br>");
//
//            // The HTTP response ends with another blank line:
//            client.println();
//            // break out of the while loop:
//            break;
//          } else {    // if you got a newline, then clear currentLine:
//            currentLine = "";
//          }
//        } else if (c != '\r') {  // if you got anything else but a carriage return character,
//          currentLine += c;      // add it to the end of the currentLine
//        }
//
//        // Check to see if the client request was "GET /H" or "GET /L":
//        if (currentLine.endsWith("GET /H")) {
//          digitalWrite(LED_BUILTIN, HIGH);               // GET /H turns the LED on
//        }
//        if (currentLine.endsWith("GET /L")) {
//          digitalWrite(LED_BUILTIN, LOW);                // GET /L turns the LED off
//        }
//      }
//    }
//    // close the connection:
//    client.stop();
//    Serial.println("Client Disconnected.");
//  }
//  //  digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
//  //  delay(1000);                       // wait for a second
//  //  digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
//  //  delay(1000);                       // wait for a second
//}

