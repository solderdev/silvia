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
#include "helpers.hpp"


static WaterControl *instance = nullptr;


WaterControl::WaterControl() :
  state_(WATERCTRL_OFF),
  pump_override_percent_(0),
  pump_override_total_ms_(0),
  pump_override_active_ms_(0)
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

void WaterControl::overridePump(uint8_t percent, uint16_t time_ms)
{
  pump_override_percent_ = percent,
  pump_override_total_ms_ = time_ms;
  pump_override_active_ms_ = 0;
}

void WaterControl::overridePumpCheck(void)
{
  static unsigned long time_last_check = 0;

  unsigned long now = systime_ms();

  if (pump_override_total_ms_ > 0)
    pump_override_active_ms_ += now - time_last_check;

  time_last_check = now;
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

void WaterControl::startSteam(uint8_t pump_percent, bool new_state_valve)
{
  if (state_ != WATERCTRL_STEAM)
    stop(0, false, WATERCTRL_STEAM);

  pump_->setPWM(pump_percent);
  if (new_state_valve)
    valve_->on();
  else
    valve_->off();

  pid_boiler_->setTarget(STEAM_TEMP, PID_MODE_STEAM);
}

void WaterControl::stop(uint8_t new_pump_percent, bool new_state_valve, WATERCTRL_State_t new_state)
{
  pid_boiler_->setTarget(BREW_TEMP, PID_MODE_WATER);

  if (pump_override_total_ms_ > pump_override_active_ms_)
  {
    pump_->setPWM(pump_override_percent_);
  }
  else
  {
    pump_override_total_ms_ = 0;
  
    shot_->stop(new_pump_percent, new_state_valve);
    preheat_->stop(new_pump_percent, new_state_valve);
    pump_->setPWM(new_pump_percent);
  }

  if (new_state_valve)
    valve_->on();
  else
    valve_->off();

  state_ = new_state;
}
