/**
 * board: NodeMCU-32S
 * flash freq: 80MHz
 * upload speed: 921600 b
 */
#include "private_defines.h"
#include "pid.h"
#include "shot.h"

//int LED_BUILTIN = 2;
#define PIN_LED_GREEN  25
#define LED_ON     digitalWrite(PIN_LED_GREEN, HIGH)
#define LED_OFF    digitalWrite(PIN_LED_GREEN, LOW)

#define BREW_TEMP_DEFAULT     98.0f
#define STEAM_TEMP_DEFAULT    125.0f
#define BREWHEAD_TEMP_TARGET  90.0f

// default pump percent in filter-coffee mode
#define PUMP_FILTER_DEFAULT  20

uint32_t lastmilli = 0;
static uint32_t power_state = 0;

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  digitalWrite(PIN_LED_GREEN, HIGH);
  Serial.begin(115200);
  delay(100);

  WIFI_setup();
  SENSORS_setup();
  SSRCTRL_setup();
  SHOT_setup();
  PREHEAT_setup();
  if (PID_setup(40, 100, 1.5, 0, 1000))  // P+, P-, I, D, t_s   
    Serial.println("error init pid");
  if (BTN_setup())
    Serial.println("error init buttons");
  //DISPLAY_setup();
  PID_stop();
  SSRCTRL_off();
  
  lastmilli = millis();
}

void loop()
{
//  static int percent_p_test = 0;
  static uint8_t led_state = 1;
  static uint8_t power_trigger_available = 1;
  
  if (millis() - lastmilli > 500 )
  {
    lastmilli = millis();
//    Serial.println(String("Btn PWR: ") + String(BTN_getButtonStatePower()) + 
//                   String(" Sw Coffee: ") + String(BTN_getSwitchStateCoffee()) + 
//                   String(" Sw Water: ") + String(BTN_getSwitchStateWater()) + 
//                   String(" Sw Steam: ") + String(BTN_getSwitchStateSteam()));
//    Serial.println(WiFi.RSSI());
    // Serial.println("Boiler side: " + String(SENSORS_get_temp_boiler_side()) + 
    //                 "C / Boiler top " + String(SENSORS_get_temp_boiler_top()) + 
    //                 "C / Brewhead " + String(SENSORS_get_temp_brewhead()) + "C / ");
  
    if (SSRCTRL_getState() == false)
    {
      // machine off
      digitalWrite(PIN_LED_GREEN, LOW);
      led_state = 0;
    }
    else if (SENSORS_get_temp_boiler_avg() < PID_getTargetTemp()-1 ||
             SENSORS_get_temp_boiler_avg() > PID_getTargetTemp()+1 ||
             SENSORS_get_temp_brewhead() < BREWHEAD_TEMP_TARGET)
    {
      // temperature not optimal
      if (led_state)
        digitalWrite(PIN_LED_GREEN, HIGH);
      else
        digitalWrite(PIN_LED_GREEN, LOW);
      led_state = !led_state;
    }
    else
    {
      // temperature optimal for shot
      digitalWrite(PIN_LED_GREEN, HIGH);
      led_state = 1;
    }
  }

  if (BTN_getButtonStatePower() == true && power_trigger_available)
  {
    power_trigger_available = 0;
    
    if (power_state)
      PWR_powerOff();
    else
      PWR_powerOn();
  }
  else if (BTN_getButtonStatePower() == false)
    power_trigger_available = 1;

  // auto power-down
  if (power_state != 0 && power_state + 50u * 60u * 1000u < millis())
  {
    // on for more than 50 min
    Serial.print("AUTO ");
    PWR_powerOff();
  }

  if (power_state)
  {
    // Coffee  Water  Steam
    // ---------------------
    //    0      0      0    all off
    //    0      0      1    temp: steam
    //    0      1      1    temp: steam, pump 50%
    //    0      1      0    pump 50%
    //    1      0      0    start shot
    //    1      1      0    valve open, pump 100%
    //    1      0      1    valve open, pump x% (filter-coffee)
    //    1      1      1    start preheat
  
    if (BTN_getSwitchStateCoffee() == true && SENSORS_get_temp_boiler_side() < 110)
    {
      power_state = millis();  // re-set power-off timer
      
      // coffee switch on and ready to brew
      if (BTN_getSwitchStateWater() == false && BTN_getSwitchStateSteam() == false)
      {
        SHOT_start(300, 3000, 4000, 10, 30);  // init-100p-t, ramp-t, pause-t, min-%, max-%
      }
      else if (BTN_getSwitchStateWater() == true && BTN_getSwitchStateSteam() == false)
      {
        // water flush mode
        SHOT_stop(true, 100);
        PREHEAT_stop(true, 100);
        SSRCTRL_set_state_valve(true);
        SSRCTRL_set_pwm_pump(100);
      }
      else if (BTN_getSwitchStateWater() == false && BTN_getSwitchStateSteam() == true)
      {
        // filter-coffee mode
        SHOT_stop(true, PUMP_FILTER_DEFAULT);
        PREHEAT_stop(true, PUMP_FILTER_DEFAULT);
        SSRCTRL_set_state_valve(true);
        SSRCTRL_set_pwm_pump(PUMP_FILTER_DEFAULT);
        SHOT_manuallyStartShot();
      }
      else if (BTN_getSwitchStateWater() == true && BTN_getSwitchStateSteam() == true)
      {
        //preheat: valve open - 100% pump (2s) - valve close - delay (800ms) - 0% pump .. repeat every 10s
        SHOT_stop(true, 100);
        PREHEAT_start(1000, 800, 10000);  // ms 100% pump , ms build pressure, ms pause)
      }
    }
    else if (BTN_getSwitchStateCoffee() == false)
    {
      // coffee switch off:
      // close valve, stop shot/preheat if active
      SHOT_stop(false, 0);
      PREHEAT_stop(false, 0);
      SSRCTRL_set_state_valve(false);
      if (BTN_getSwitchStateWater() == true)
      {
        power_state = millis();  // re-set power-off timer
        SSRCTRL_set_pwm_pump(50);
      }
      else
        SSRCTRL_set_pwm_pump(0);
    }
    else
    {
      SHOT_stop(false, 0);
      PREHEAT_stop(false, 0);
      SSRCTRL_set_state_valve(false);
      SSRCTRL_set_pwm_pump(0);
    }
    
    if (BTN_getSwitchStateSteam() == true && BTN_getSwitchStateCoffee() == false)
      PID_setTargetTemp(STEAM_TEMP_DEFAULT, PID_MODE_STEAM);
    else
      PID_setTargetTemp(BREW_TEMP_DEFAULT, PID_MODE_WATER);
  } /* if power_state */
  
  //DISPLAY_service();

//  if (BTN_getDDcw())
//    Serial.println("CW");
//  if (BTN_getDDccw())
//    Serial.println("CCW");

  yield();
}

void PWR_powerOn(void)
{
  Serial.println("powering UP!");
  power_state = millis();
  PID_start();
  SSRCTRL_on();
}

void PWR_powerOff(void)
{
  Serial.println("powering DOWN!");
  power_state = 0;
  PID_stop();
  SSRCTRL_off();
}

uint32_t PWR_isPoweredOn(void)
{
  return power_state;
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
