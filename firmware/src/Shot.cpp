#include "Shot.hpp"
#include "WaterControl.hpp"
#include "SSR.hpp"
#include "SSRPump.hpp"
#include "PIDHeater.hpp"
#include "helpers.hpp"
#include "TaskConfig.hpp"

Shot::Shot(WaterControl *water_control) :
  water_control_(water_control),
  time_init_fill_ms_(0),
  time_ramp_ms_(0),
  time_pause_ms_(0),
  pump_start_percent_(0),
  pump_stop_percent_(0),
  current_ramp_percent_(0),
  active_(false)
{
  start_time_ = systime_ms();
  stop_time_ = start_time_;

  cmd_queue_ = xQueueCreate(cmd_queue_size_, cmd_queue_item_size_);

  // crete timer for delayed execution of commands
  timer_infos_.cmd = CMD_STOP;
  timer_infos_.shot = this;
  timer_ = xTimerCreate("tmr_shot", pdMS_TO_TICKS(1), pdFALSE, &timer_infos_, &Shot::timer_cb_wrapper);

  BaseType_t rval = xTaskCreate(&Shot::task_wrapper, "task_shot", TaskConfig::Shot_stacksize, this, TaskConfig::Shot_priority, &task_handle_);

  if (timer_ == NULL || cmd_queue_ == NULL || rval != pdPASS )
  {
    Serial.println("Shot ERROR init failed");
    return; // error
  }
}
void Shot::task_wrapper(void *arg)
{
  static_cast<Shot *>(arg)->task();
}
void Shot::task()
{
  uint32_t command;

  while (1)
  {
    if (xQueueReceive(cmd_queue_, &command, portMAX_DELAY))
    {
      // start: enable valve
      // ramp up pump from pump_percent_start to 100% in time_ramp seconds
      //   increment_duration = time_ramp_ms / ((pump_stop_percent - pump_start_percent)/10)
      //   
      // continue with 100% until stop
      switch (command)
      {
        case CMD_STOP:
          stop_time_ = systime_ms();
          break;

        case CMD_START:
          start_time_ = 0;
          water_control_->valve_->on();
          water_control_->pump_->setPWM(100);
          current_ramp_percent_ = pump_start_percent_;
          // 100% pump for time_init_fill_ms_
          xTimerChangePeriod(timer_, pdMS_TO_TICKS(time_init_fill_ms_), portMAX_DELAY);
          timer_infos_.cmd = CMD_RAMP;
          break;

        case CMD_RAMP:
          if (!active_)
            break;
          // check if we are done with ramp
          if (current_ramp_percent_ > pump_stop_percent_ || current_ramp_percent_ >= 100)
          {
            // ramp finished
            uint32_t cmd = CMD_PAUSE;
            xQueueSendToBack(cmd_queue_, &cmd, portMAX_DELAY);
            break;
          }
          
          // engage PID override in second stage of ramp, so it's not triggered by a short flip of the switch
          if (current_ramp_percent_ > pump_start_percent_)
            water_control_->pid_boiler_->overrideOutput(PID_OVERRIDE_STARTSHOT, PID_OVERRIDE_COUNT);
            
          water_control_->pump_->setPWM(current_ramp_percent_);
          current_ramp_percent_ += 10;
          
          xTimerChangePeriod(timer_, pdMS_TO_TICKS(time_ramp_ms_ / ((pump_stop_percent_ - pump_start_percent_)/10)), portMAX_DELAY);
          timer_infos_.cmd = CMD_RAMP;
          break;

        case CMD_PAUSE:
          if (!active_)
            break;
          // pause or 100%
          if (time_pause_ms_ > 0)
          {
            water_control_->pump_->setPWM(0);
            xTimerChangePeriod(timer_, pdMS_TO_TICKS(time_pause_ms_), portMAX_DELAY);
            timer_infos_.cmd = CMD_100PERCENT;
          }
          else
          {
            // go directly to 100%
            uint32_t cmd = CMD_100PERCENT;
            xQueueSendToBack(cmd_queue_, &cmd, portMAX_DELAY);
          }
          break;

        case CMD_100PERCENT:
          if (!active_)
            break;
          stop_time_ = 0;
          start_time_ = systime_ms();
          water_control_->pump_->setPWM(100);
          break;
        
        default:
          Serial.println("Shot ERROR command unkown");
          break;
      }
    }
  }
}

void Shot::timer_cb_wrapper(TimerHandle_t arg)
{
  TimerInfos_t *timer_infos = static_cast<TimerInfos_t *>(pvTimerGetTimerID(arg));
  timer_infos->shot->timer_cb(timer_infos->cmd);
}
void Shot::timer_cb(uint32_t cmd)
{
  if (xQueueSendToBack(cmd_queue_, &cmd, 0) != pdPASS )
    Serial.println("Shot ERROR timed cb queue send");
}

void Shot::start(uint32_t init_fill_ms, uint32_t time_ramp_ms, uint32_t time_pause_ms, 
                         uint8_t pump_start_percent, uint8_t pump_stop_percent)
{
  if (active_ ||
      init_fill_ms == 0 || init_fill_ms > 5000 ||
      time_ramp_ms > 10000 || time_pause_ms > 10000 ||
      pump_start_percent > 100 || pump_stop_percent > 100 || pump_start_percent > pump_stop_percent)
  {
    Serial.println("Shot start: invalid parameters");
    return;
  }
  Serial.println("Shot: starting");
  xTimerStop(timer_, portMAX_DELAY);
  active_ = true;
  
  time_init_fill_ms_ = init_fill_ms;
  time_ramp_ms_ = time_ramp_ms;
  time_pause_ms_ = time_pause_ms;
  pump_start_percent_ = pump_start_percent;
  pump_stop_percent_ = pump_stop_percent;
  
  uint32_t cmd = CMD_START;
  xQueueSendToBack(cmd_queue_, &cmd, portMAX_DELAY);
}

void Shot::stop(uint8_t pump_percent, bool valve)
{
  if (!active_)
    return;

  Serial.println("Shot: stopping");
  active_ = false;
  xTimerStop(timer_, portMAX_DELAY);

  uint32_t cmd = CMD_STOP;
  xQueueSendToBack(cmd_queue_, &cmd, portMAX_DELAY);
  
  if (valve)
    water_control_->valve_->on();
  else
    water_control_->valve_->off();
  water_control_->pump_->setPWM(pump_percent);
}

uint32_t Shot::getShotTime()
{
  if (start_time_ == 0)
  {
    // pre-infusion
    return 0;
  }
  else if (stop_time_ == 0)
  {
    // during shot
    return systime_ms() - start_time_;
  }
  else if (stop_time_ < start_time_)
  {
    Serial.println("Shot ERROR: start and stop time wrong");
    return 0;
  }
  else
  {
    // after shot stopped
    return stop_time_ - start_time_;
  }
}
