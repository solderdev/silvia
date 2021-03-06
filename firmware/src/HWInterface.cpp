#include <Arduino.h>
#include "HWInterface.hpp"
#include "WaterControl.hpp"
#include "Button.hpp"
#include "Pins.hpp"
#include "coffee_config.hpp"
#include "helpers.hpp"

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
  if (power_state_ != 0 && (unsigned long)(power_state_ + POWEROFF_MINUTES * 60u * 1000u) < systime_ms())
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

    bool sw_coffee_active = sw_coffee->active();
    bool sw_water_active = sw_water->active();
    bool sw_steam_active = sw_steam->active();

    if (sw_coffee_active || sw_water_active || sw_steam_active)
    {
      // disable pump override when any button is pressed
      water_control_->overridePump(0, 0);
    }
    water_control_->overridePumpCheck();
  
    if (sw_coffee_active)
    {
      power_state_ = systime_ms();  // re-set power-off timer
      
      // coffee switch on and ready to brew
      if (sw_water_active == false && sw_steam_active == false)
      {
        water_control_->startShot();
      }
      else if (sw_water_active == false && sw_steam_active)
      {
  //       // filter-coffee mode
  // TODO - timer      SHOT_manuallyStartShot();
        water_control_->startPump(20, true);
      }
      else if (sw_water_active && sw_steam_active == false)
      {
        // water flush mode
        water_control_->startPump(100, true);
      }
      else if (sw_water_active && sw_steam_active)
      {
        water_control_->startPreheat();
      }
      else
        Serial.println("ERROR: Button state not reachable (coffee on)!");
    }
    else
    {
      // coffee switch off:
      if (sw_water_active == false && sw_steam_active == false)
      {
        water_control_->stop();
      }
      else if (sw_water_active == false && sw_steam_active)
      {
        water_control_->startSteam();
      }
      else if (sw_water_active && sw_steam_active == false)
      {
        power_state_ = systime_ms();  // re-set power-off timer
        water_control_->startPump(50, false);
      }
      else if (sw_water_active && sw_steam_active)
      {
        water_control_->startSteam(0, true);
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
  power_state_ = systime_ms();
  water_control_->enable();
  digitalWrite(Pins::led_green, HIGH);
}
