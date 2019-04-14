#include <Arduino.h>
#include "WaterControl.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "SSRPump.hpp"
#include "SSR.hpp"
#include "Pins.hpp"
#include "Timers.hpp"
#include "Shot.hpp"
#include "Preheat.hpp"
#include "PIDHeater.hpp"
#include "coffee_config.hpp"


static WaterControl *instance = nullptr;


WaterControl::WaterControl() :
  state_(WATERCTRL_OFF)
{
  if (instance)
  {
    Serial.println("ERROR: more than one WaterControls generated");
    ESP.restart();
    return;
  }

  pump_ = new SSRPump(Pins::ssr_pump, Timers::timer_pump, 20000);
  pid_boiler_ = new PIDHeater(this);
  valve_ = new SSR(Pins::ssr_valve);
  shot_ = new Shot(this);
  preheat_ = new Preheat(this);

  instance = this;
}

WaterControl* WaterControl::getInstance()
{
  return instance;
}

void WaterControl::enable()
{
  stop();

  pid_boiler_->start();
  pump_->enable();
  valve_->enable();
}

void WaterControl::disable()
{
  stop();

  pid_boiler_->stop();
  pump_->disable();
  valve_->disable();
}

void WaterControl::startPump(uint8_t percent, bool valve)
{
  if (valve)
    stop(percent, valve, WATERCTRL_WATER_VALVE);
  else
    stop(percent, valve, WATERCTRL_WATER);
}

void WaterControl::startShot()
{
  if (state_ == WATERCTRL_SHOT)
    return;
  
  // keep pump and valve on, if already running
  stop(100, true, WATERCTRL_SHOT);

  shot_->start(SHOT_T_INITWATER, SHOT_T_RAMP, SHOT_T_PAUSE, SHOT_RAMP_MIN, SHOT_RAMP_MAX);  // init-100p-t, ramp-t, pause-t, min-%, max-%
}

uint32_t WaterControl::getShotTime()
{
  return shot_->getShotTime();
}

void WaterControl::startPreheat()
{
  if (state_ == WATERCTRL_PREHEAT)
    return;
  
  // keep pump and valve on, if already running
  stop(100, true, WATERCTRL_PREHEAT);

  //preheat: valve open - 100% pump - valve close + delay (800ms) - 0% pump .. repeat every 10s
  preheat_->start(PREHEAT_T_WATER, PREHEAT_T_FILL, PREHEAT_T_PAUSE);  // ms 100% pump , ms build pressure, ms pause)
}

void WaterControl::startSteam(uint8_t pump_percent)
{
  if (state_ != WATERCTRL_STEAM)
    stop(0, false, WATERCTRL_STEAM);

  pump_->setPWM(pump_percent);

  pid_boiler_->setTarget(STEAM_TEMP, PID_MODE_STEAM);
}

void WaterControl::stop(uint8_t new_pump_percent, bool new_state_valve, WATERCTRL_State_t new_state)
{
  pid_boiler_->setTarget(BREW_TEMP, PID_MODE_WATER);
  
  shot_->stop(new_pump_percent, new_state_valve);
  preheat_->stop(new_pump_percent, new_state_valve);

  pump_->setPWM(new_pump_percent);
  if (new_state_valve)
    valve_->on();
  else
    valve_->off();

  state_ = new_state;
}
