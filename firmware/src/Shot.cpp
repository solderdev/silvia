#include "Shot.hpp"
#include "WaterControl.hpp"
#include "SSR.hpp"
#include "SSRPump.hpp"
#include "PIDHeater.hpp"

Shot::Shot(WaterControl *water_control) :
  water_control_(water_control),
  state_(SHOT_OFF),
  time_init_fill_ms_(0),
  time_ramp_ms_(0),
  time_pause_ms_(0),
  pump_start_percent_(0),
  pump_stop_percent_(0),
  current_ramp_percent_(0),
  mux_(portMUX_INITIALIZER_UNLOCKED)
{
  start_time_ = millis();
  stop_time_ = millis();

  // shot timer - start must be called separately
  timer_ = xTimerCreate("tmr_shot", pdMS_TO_TICKS(1), pdFALSE, this, &Shot::timer_cb_wrapper);
  if (timer_ == NULL)
  {
    Serial.println("Shot ERROR timer init failed");
    return; // error
  }
}

void Shot::timer_cb_wrapper(TimerHandle_t arg)
{
  static_cast<Shot *>(pvTimerGetTimerID(arg))->timer_cb();
}

// start: enable valve
// ramp up pump from pump_percent_start to 100% in time_ramp seconds
//   increment_duration = time_ramp_ms / ((pump_stop_percent - pump_start_percent)/10)
//   
// continue with 100% until stop()
void Shot::timer_cb()
{
  //Serial.println("Shot state " + String(state_));
  
  portENTER_CRITICAL(&mux_);
  switch (state_)
  {
    case SHOT_OFF:
      // water_control_->pump_->setPWM(0);
      // water_control_->valve_->off();
      // xTimerStop(timer_, 0);
      portEXIT_CRITICAL(&mux_);
      Serial.println("Shot ERROR timer called in off state");
      break;
      
    case SHOT_INIT_FILL:
      // init fill is finished: go to ramp
      state_ = SHOT_RAMP;
      portEXIT_CRITICAL(&mux_);
      current_ramp_percent_ = pump_start_percent_;
      water_control_->pump_->setPWM(current_ramp_percent_);
      xTimerChangePeriod(timer_, pdMS_TO_TICKS(time_ramp_ms_ / ((pump_stop_percent_ - pump_start_percent_)/10)), 0);
      break;
      
    case SHOT_RAMP:
      current_ramp_percent_ += 10;
      if (current_ramp_percent_ > pump_stop_percent_)
      {
        // ramp finished - continue with pause or 100%
        current_ramp_percent_ = 0;
        if (time_pause_ms_ > 0)
        {
          state_ = SHOT_PAUSE;
          portEXIT_CRITICAL(&mux_);
          water_control_->pump_->setPWM(0);
          xTimerChangePeriod(timer_, pdMS_TO_TICKS(time_pause_ms_), 0);
        }
        else
        {
          // go directly to 100% (same as case SHOT_PAUSE)
          state_ = SHOT_100PERCENT;
          portEXIT_CRITICAL(&mux_);
          start_time_ = millis();
          stop_time_ = 0;
          water_control_->pump_->setPWM(100);
          water_control_->pid_boiler_->overrideOutput(PID_OVERRIDE_STARTSHOT, PID_OVERRIDE_COUNT);
        }
        break;
      }
      else
        portEXIT_CRITICAL(&mux_);
      // continue with new ramp value
      water_control_->pump_->setPWM(current_ramp_percent_);
      xTimerChangePeriod(timer_, pdMS_TO_TICKS(time_ramp_ms_ / ((pump_stop_percent_ - pump_start_percent_)/10)), 0);
      break;
      
    case SHOT_PAUSE:
      // pause done - continue with 100%
      state_ = SHOT_100PERCENT;
      portEXIT_CRITICAL(&mux_);
      start_time_ = millis();
      stop_time_ = 0;
      water_control_->pump_->setPWM(100);
      water_control_->pid_boiler_->overrideOutput(PID_OVERRIDE_STARTSHOT, PID_OVERRIDE_COUNT);
      break;
      
    case SHOT_100PERCENT:
      portEXIT_CRITICAL(&mux_);
      break;

    case SHOT_MANUAL:
      portEXIT_CRITICAL(&mux_);
      // TODO - unused?
      break;
      
    default:
      portEXIT_CRITICAL(&mux_);
      Serial.println("Shot ERROR state");
      break;
  }
}

void Shot::start(uint32_t init_fill_ms, uint32_t time_ramp_ms, uint32_t time_pause_ms, 
                         uint8_t pump_start_percent, uint8_t pump_stop_percent)
{
  if (init_fill_ms == 0 || init_fill_ms > 5000 ||
      time_ramp_ms > 10000 || time_pause_ms > 10000 ||
      pump_start_percent > 100 || pump_stop_percent > 100 || pump_start_percent > pump_stop_percent)
    return;
    
  if (state_ == SHOT_OFF)
  {
    Serial.println("Shot: starting");
    time_init_fill_ms_ = init_fill_ms;
    time_ramp_ms_ = time_ramp_ms;
    time_pause_ms_ = time_pause_ms;
    pump_start_percent_ = pump_start_percent;
    pump_stop_percent_ = pump_stop_percent;
    
    start_time_ = millis();
    state_ = SHOT_INIT_FILL;
    water_control_->valve_->on();
    water_control_->pump_->setPWM(100);
    // 100% pump for time_init_fill seconds
    xTimerChangePeriod(timer_, pdMS_TO_TICKS(time_init_fill_ms_), portMAX_DELAY);
  }
}

void Shot::stop(uint8_t pump_percent, bool valve)
{
  if (state_ != SHOT_OFF)
  {
    Serial.println("Shot: stopping");
    xTimerStop(timer_, portMAX_DELAY);
    state_ = SHOT_OFF;
    if (valve)
      water_control_->valve_->on();
    else
      water_control_->valve_->off();
    water_control_->pump_->setPWM(pump_percent);
    stop_time_ = millis();
  }
}

uint32_t Shot::getShotTime()
{
  if (stop_time_ == 0)
    return millis() - start_time_;
  else if (stop_time_ < start_time_)
  {
    Serial.println("Shot ERROR: start and stop time wrong");
    return 0;
  }
  else
    return stop_time_ - start_time_;
}
