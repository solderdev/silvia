#include <Arduino.h>
#include "Sensors.hpp"
#include "HWInterface.hpp"
#include "WaterControl.hpp"
#include "WebInterface.hpp"
#include "Pins.hpp"
#include "helpers.hpp"

#define CORE_DEBUG_LEVEL 5

HWInterface *hw_interface;
WaterControl *water_control;
SensorsHandler *sensors_handler;
WebInterface *web_interface;

void setup()
{
  systime_init();
  
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(Pins::led_green, OUTPUT);
  digitalWrite(Pins::led_green, HIGH);

  Serial.begin(115200);
  delay(1000);

  Serial.println("Hi there! Booting now..");

  sensors_handler = new SensorsHandler();
  water_control = new WaterControl();
  hw_interface = new HWInterface(water_control);
  web_interface = new WebInterface();
  
  Serial.println("setup done");
}

void loop()
{
  // service the button interface
  hw_interface->service();

  vTaskDelay(pdMS_TO_TICKS(10));
}
