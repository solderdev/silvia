#include "PIDHeater.hpp"
#include "SSRHeater.hpp"
#include "SSRPump.hpp"
#include "WaterControl.hpp"
#include "Pins.hpp"
#include "Timers.hpp"
#include "TaskConfig.hpp"
#include "Sensors.hpp"
#include "WebInterface.hpp"

PIDHeater::PIDHeater(WaterControl *water_control, float p_pos, float p_neg, float i, float d, uint32_t ts_ms) :
  water_control_(water_control),
  kPpos_(p_pos),
  kPneg_(p_neg),
  kI_(i),
  kD_(d),
  ts_(ts_ms),
  u_(0.0f),
  p_share_(0.0f),
  i_share_(0.0f),
  d_share_(0.0f),
  u1_(0.0f),
  pv1_(0.0f),
  pv2_(0.0f),
  target_(0.0f),
  mode_(PID_MODE_WATER),
  u_override_(-1.0f),
  u_override_cnt_(0),
  enabled_(false),
  sem_update_(nullptr),
  timer_update_(nullptr),
  task_handle_(nullptr)
{
  heater_ = new SSRHeater(Pins::ssr_heater, Timers::timer_heater, 10000);

  // sync semaphore
  sem_update_ = xSemaphoreCreateBinary();
  
  // update timer - start must be called separately
  timer_update_ = xTimerCreate("tmr_pid", pdMS_TO_TICKS(ts_), pdTRUE, this, &PIDHeater::timer_cb_wrapper);

  // create task
  BaseType_t rval = xTaskCreate(&PIDHeater::task_wrapper, "task_pid", TaskConfig::PIDHeater_stacksize, this, TaskConfig::PIDHeater_priority, &task_handle_);

  if (sem_update_ == NULL || timer_update_ == NULL || rval != pdPASS || task_handle_ == NULL)
  {
    Serial.println("PIDHeater ERROR init failed");
  }
}

void PIDHeater::start()
{
  Serial.println("PIDHeater ON!");
  pv2_= SensorsHandler::getInstance()->getTempBoilerAvg();
  pv1_ = pv2_;
  u1_ = 0.0f;
  
  heater_->sync();
  xTimerReset(timer_update_, 0);

  enabled_ = true;
  Serial.println("P+ = " + String(kPpos_) + " P- = " + String(kPneg_) + " I = " + String(kI_) + " D = " + String(kD_));

  heater_->enable();
}

void PIDHeater::stop()
{
  enabled_ = false;
  heater_->setPWM(0);
  heater_->disable();
  Serial.println("PIDHeater OFF!");
}

void PIDHeater::overrideOutput(float u_override, int8_t count)
{
  u_override_ = u_override;
  u_override_cnt_ = count;
}

void PIDHeater::setTarget(float temp, PID_Mode_t mode)
{
  if (temp > PID_MIN_TEMP && temp < PID_MAX_TEMP)
    target_ = temp;
  else
    target_ = PID_MIN_TEMP;

  mode_ = mode;
}

void PIDHeater::timer_cb_wrapper(TimerHandle_t arg)
{
  static_cast<PIDHeater *>(pvTimerGetTimerID(arg))->timer_cb();
}
void PIDHeater::timer_cb()
{
  xSemaphoreGive(sem_update_);
}

void PIDHeater::task_wrapper(void *arg)
{
  static_cast<PIDHeater *>(arg)->task();
}
void PIDHeater::task()
{
  float pv, e;
  float u_limited;

  xTimerStart(timer_update_, portMAX_DELAY);

  while(1)
  {
    if (xSemaphoreTake(sem_update_, portMAX_DELAY) == pdTRUE)
    {
      if (enabled_ == true)
      {
        // Serial.println("PID running at " + String(millis()));
        if (mode_ == PID_MODE_WATER)
        {
          float top = SensorsHandler::getInstance()->getTempBoilerTop();
          float side = SensorsHandler::getInstance()->getTempBoilerSide();
          float average = SensorsHandler::getInstance()->getTempBoilerAvg();
          if (side > top)
            pv = side;
          else
            pv = average;
        }
        else
          pv = SensorsHandler::getInstance()->getTempBoilerMax();
          
        e = target_ - pv;

        // from Wikipedia (same as type A)
        // float e1 = target_ - pid_pv1;  // for type A and B
        // float e2 = target_ - pid_pv2;
        // p_share_ = kP * (e - e1);
        // i_share_ = kI * ((float)(ts_)/1000.0f) * e;
        // d_share_ = (kD * (e - 2*e1 + e2)) / ((float)(ts_)/1000.0f);
        
        // PID type C
        // always use less defensive P+ value in steam mode
        if ((pv1_ - pv) > 0 || mode_ == PID_MODE_STEAM)
          p_share_ = kPpos_ * (pv1_ - pv);
        else
          p_share_ = kPneg_ * (pv1_ - pv);
        i_share_ = kI_ * ((float)(ts_)/1000.0f) * e;
        d_share_ = (kD_ * (2*pv1_ - pv - pv2_)) / ((float)(ts_)/1000.0f);
        
        u_ = u1_ + p_share_ + i_share_ + d_share_;
        // keep calculated u_ value separate from modifications for data-logging
        u_limited = u_;
     
#if 1   // modifications/overrides to default PID
        // faster heat-up, if far too cold (10*C)
        if (e > 10)
          u_limited = 100;

        if (water_control_->pump_->getPWM() == PWM_0_PERCENT)
        {
          // limit heater, if pump is off and we are hotter than SP
          if (u_limited > 5 && pv >= target_ + 0.5)
            u_limited = 5;
        }
        else if (water_control_->pump_->getPWM() == PWM_100_PERCENT && u_limited < 20 && e > 0)
        {
          // set a minimum of 20% heater during a shot, when temp is too low
          // u_limited = 20;
        }

        // apply override value if activated
        if (u_override_ >= 0.0f && u_override_cnt_ > 0)
        {
          u_limited = u_override_;
          u_override_cnt_--;
        }
#endif

        // anti-windup and safety
        if (u_limited < 0)
          u_limited = 0;
        else if (u_limited > 100)
          u_limited = 100;

        // check again, if we got interrupted (not perfect, but better)
        if (enabled_)
        {
          uint32_t set_value = lroundf(u_limited);
          // if the PID output is positive, apply a minimum value
          // otherwise heater is too slow to react and system is instable
          if (set_value == 0)
            heater_->setPWM(0);
          else if (set_value <= PID_MIN_OUTPUT)
            heater_->setPWM(PID_MIN_OUTPUT);  // minimum heater output
          else
            heater_->setPWM(set_value);
        }
    
        // thermostat simulation
        // if (pv < target_temp)
        //   heater_->setPWM(100);
        // else
        //   heater_->setPWM(0);
    
        // save old values
        pv2_ = pv1_;
        pv1_ = pv;
        u1_ = u_limited;
      }  // end of pid_enabled
      else
      {
        p_share_ = 0;
        i_share_ = 0;
        d_share_ = 0;
        u_ = 0;
      }
                       
      WebInterface::updateInfluxDB();

      // Serial.println(String(millis()) + " , " + 
      //                 String(target_) + " , " + 
      //                 String(SensorsHandler::getTempBoilerSide()) + " , " + 
      //                 String(SensorsHandler::getTempBoilerTop()) + " , " + 
      //                 String(SensorsHandler::getTempBrewhead()) + " , " + 
      //                 String(SSRHeater::getInstance()->getPWM()) + " , " + 
      //                 String(SSRPump::getInstance()->getPWM()) + " , " + 
      //                 String(u_) + " , " + 
      //                 String(p_share_) + " , " + 
      //                 String(i_share_) + " , " + 
      //                 String(d_share_) );
    }  // end of sem take
  }  // end of while(1)
}
