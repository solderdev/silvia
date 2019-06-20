#include <Arduino.h>
#include "HWInterface.hpp"
#include "WaterControl.hpp"
#include "Button.hpp"
#include "Pins.hpp"
#include "coffee_config.hpp"

static HWInterface *instance = nullptr;

HWInterface::HWInterface(WaterControl *water_control) :
  water_control_(water_control),
  power_trigger_available_(true),
  power_state_(0)
{
  if (instance)
  {
    Serial.println("ERROR: more than one HWInterface generated");
    ESP.restart();
    return;
  }

  btn_power = new Button(Pins::button_power, INPUT);
  sw_coffee = new Button(Pins::switch_coffee, INPUT);
  sw_water = new Button(Pins::switch_water, INPUT);
  sw_steam = new Button(Pins::switch_steam, INPUT);

  water_control_->disable();
  digitalWrite(Pins::led_green, LOW);

  instance = this;
}

HWInterface* HWInterface::getInstance()
{
  return instance;
}

void HWInterface::service()
{
  if (btn_power->active())
  {
    if (power_trigger_available_)
    {
      power_trigger_available_ = false;
      
      if (power_state_)
        powerOff();
      else
        powerOn();
    }
  }
  else
    power_trigger_available_ = 1;

  // auto power-down
  if (power_state_ != 0 && (unsigned long)(power_state_ + POWEROFF_MINUTES * 60u * 1000u) < millis())
  {
    // on for more than 50 min
    Serial.print("AUTO ");
    powerOff();
  }

  if (power_state_)
  {
    // Coffee  Water  Steam
    // ---------------------
    //    0      0      0    all off
    //    0      0      1    temp: steam
    //    0      1      0    pump 50%
    //    0      1      1    temp: steam, pump 50%
    //    1      0      0    start shot
    //    1      0      1    valve open, pump x% (filter-coffee)
    //    1      1      0    valve open, pump 100%
    //    1      1      1    start preheat
  
    if (sw_coffee->active())
    {
      power_state_ = millis();  // re-set power-off timer
      
      // coffee switch on and ready to brew
      if (sw_water->active() == false && sw_steam->active() == false)
      {
        water_control_->startShot();
      }
      else if (sw_water->active() == false && sw_steam->active())
      {
  //       // filter-coffee mode
  // TODO - timer      SHOT_manuallyStartShot();
        water_control_->startPump(20, true);
      }
      else if (sw_water->active() && sw_steam->active() == false)
      {
        // water flush mode
        water_control_->startPump(100, true);
      }
      else if (sw_water->active() && sw_steam->active())
      {
        water_control_->startPreheat();
      }
      else
        Serial.println("ERROR: Button state not reachable (coffee on)!");
    }
    else
    {
      // coffee switch off:
      if (sw_water->active() == false && sw_steam->active() == false)
      {
        water_control_->stop();
      }
      else if (sw_water->active() == false && sw_steam->active())
      {
        water_control_->startSteam();
      }
      else if (sw_water->active() && sw_steam->active() == false)
      {
        power_state_ = millis();  // re-set power-off timer
        water_control_->startPump(50, false);
      }
      else if (sw_water->active() && sw_steam->active())
      {
        water_control_->startSteam(50);
      }
      else
        Serial.println("ERROR: Button state not reachable (coffee off)!");
    }
  } /* if power_state */
}

bool HWInterface::isActive()
{
  return (power_state_) ? true : false;
}

void HWInterface::powerOff()
{
  Serial.println("powering DOWN!");
  power_state_ = 0;
  water_control_->disable();
  digitalWrite(Pins::led_green, LOW);
}

void HWInterface::powerOn()
{
  Serial.println("powering UP!");
  power_state_ = millis();
  water_control_->enable();
  digitalWrite(Pins::led_green, HIGH);
}
