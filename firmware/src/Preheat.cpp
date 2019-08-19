#include "Preheat.hpp"
#include "WaterControl.hpp"
#include "SSR.hpp"
#include "SSRPump.hpp"

Preheat::Preheat(WaterControl *water_control) :
  water_control_(water_control),
  state_(PREHEAT_OFF),
  time_pump_ms_(0),
  time_stop_pump_ms_(0),
  time_pause_ms_(0),
  mux_(portMUX_INITIALIZER_UNLOCKED)
{
  // preheat timer - start must be called separately
  timer_ = xTimerCreate("tmr_preheat", pdMS_TO_TICKS(1), pdFALSE, this, &Preheat::timer_cb_wrapper);
  if (timer_ == NULL)
  {
    Serial.println("Preheat ERROR timer init failed");
    return; // error
  }
}

void Preheat::timer_cb_wrapper(TimerHandle_t arg)
{
  static_cast<Preheat *>(pvTimerGetTimerID(arg))->timer_cb();
}

// enable valve + 100% pump
// wait time_pump_ms_
// disable valve
// wait time_stop_pump_ms_
// 0% pump
// wait time_pause_ms_
void Preheat::timer_cb()
{
  portENTER_CRITICAL(&mux_);
  switch (state_)
  {
    case PREHEAT_OFF:
      portEXIT_CRITICAL(&mux_);
      Serial.println("Preheat ERROR timer called in off state");
      break;

    case PREHEAT_100PERCENT:
      state_ = PREHEAT_STOPPUMP;
      portEXIT_CRITICAL(&mux_);
      water_control_->pump_->setPWM(100);
      water_control_->valve_->on();
      xTimerChangePeriod(timer_, pdMS_TO_TICKS(time_pump_ms_), 0);
      break;
      
    case PREHEAT_STOPPUMP:
      state_ = PREHEAT_PAUSE;
      portEXIT_CRITICAL(&mux_);
      water_control_->valve_->off();
      xTimerChangePeriod(timer_, pdMS_TO_TICKS(time_stop_pump_ms_), 0);
      break;
      
    case PREHEAT_PAUSE:
      state_ = PREHEAT_100PERCENT;
      portEXIT_CRITICAL(&mux_);
      water_control_->valve_->off();
      water_control_->pump_->setPWM(0);
      xTimerChangePeriod(timer_, pdMS_TO_TICKS(time_pause_ms_), 0);
      break;
      
    default:
      portEXIT_CRITICAL(&mux_);
      Serial.println("Preheat ERROR state");
      break;
  }
}

void Preheat::start(uint32_t time_pump_ms, uint32_t time_stop_pump_ms, uint32_t time_pause_ms)
{
  if (time_pump_ms < 100 || time_pump_ms > 5000 ||
      time_stop_pump_ms > 2000 || 
      time_pause_ms < 4000 || time_pause_ms > 50000)
    return;

  portENTER_CRITICAL(&mux_);
  if (state_ == PREHEAT_OFF)
  {
    Serial.println("Preheat: starting");
    time_pump_ms_ = time_pump_ms;
    time_stop_pump_ms_ = time_stop_pump_ms;
    time_pause_ms_ = time_pause_ms;
    
    // call timer-cb once to start loop
    state_ = PREHEAT_100PERCENT;
    portEXIT_CRITICAL(&mux_);
    timer_cb();
  }
  else
  {
    portEXIT_CRITICAL(&mux_);
  }
}

void Preheat::stop(uint8_t pump_percent, bool valve)
{
  portENTER_CRITICAL(&mux_);
  if (state_ != PREHEAT_OFF)
  {
    Serial.println("Preheat: stopping");
    xTimerStop(timer_, portMAX_DELAY);
    state_ = PREHEAT_OFF;
    portEXIT_CRITICAL(&mux_);
    if (valve)
      water_control_->valve_->on();
    else
      water_control_->valve_->off();
    water_control_->pump_->setPWM(pump_percent);
  }
  else
  {
    portEXIT_CRITICAL(&mux_);
  }
}
