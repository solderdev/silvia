#include "SSRHeater.hpp"

static void timer_callback(void);

static SSRHeater *instance = nullptr;

SSRHeater::SSRHeater(uint8_t ctrl_pin, int32_t timer_id, uint32_t timer_period_us) : SSR(ctrl_pin),
  timer_pwm_(nullptr),
  pwm_percent_(0)
{
  if (instance)
  {
    Serial.println("ERROR: more than one heaters generated");
    ESP.restart();
    return;
  }

  timer_pwm_ = timerBegin(timer_id, 80, true);
  timerAttachInterrupt(timer_pwm_, &timer_callback, true);
  timerAlarmWrite(timer_pwm_, timer_period_us, true);
  timerAlarmEnable(timer_pwm_);

  instance = this;
}

SSRHeater* SSRHeater::getInstance()
{
  return instance;
}

void SSRHeater::sync()
{
  if (timer_pwm_)
    timerRestart(timer_pwm_);
}

void SSRHeater::setPWM(uint8_t percent)
{
  if (!enabled_ || timer_pwm_ == nullptr)
    return;
  
  if (percent > 100)
  {
    percent = 0;
    Serial.println("SSRHeater: percent > 100!");
  }
  
  pwm_percent_ = percent;
}

uint8_t IRAM_ATTR SSRHeater::getPWM()
{
  return pwm_percent_;
}

static void IRAM_ATTR timer_callback(void)
{
  static uint32_t pwm_period_counter_ = 0;  // 0 to 99 elapsed periods

  if (instance == nullptr)
    return;
  
  if (!instance->isEnabled())
  {
    pwm_period_counter_ = 0;
    instance->off();
    return;
  }

  // called every 10ms == halve sine period
  if (instance->getPWM() > pwm_period_counter_)
    instance->on();
  else
    instance->off();
  
  if (++pwm_period_counter_ >= 100)
    pwm_period_counter_ = 0;
}
