#include "SSRPump.hpp"

static void timer_callback(void);

static SSRPump *instance = nullptr;

SSRPump::SSRPump(uint8_t ctrl_pin, int32_t timer_id, uint32_t timer_period_us) : SSR(ctrl_pin),
  timer_pwm_(nullptr),
  pwm_percent_(PWM_0_PERCENT),
  time_on_(0)
{
  if (instance)
  {
    Serial.println("ERROR: more than one pumps generated");
    ESP.restart();
    return;
  }

  timer_pwm_ = timerBegin(timer_id, 80, true);
  timerAttachInterrupt(timer_pwm_, &timer_callback, true);
  timerAlarmWrite(timer_pwm_, timer_period_us, true);
  timerAlarmEnable(timer_pwm_);

  instance = this;
}

SSRPump* SSRPump::getInstance()
{
  return instance;
}

void SSRPump::setPWM(uint8_t percent)
{
  if (!enabled_ || timer_pwm_ == nullptr)
    return;
  
  if (percent < 5)
  {
    if (pwm_percent_ != PWM_0_PERCENT && millis() - time_on_ > 3000)
    {
      // if (SHOT_getState() != SHOT_PAUSE)
      //   PID_override(0.0f, PID_OVERRIDE_CNT);
    }
    pwm_percent_ = PWM_0_PERCENT;
  }
  else
  {
    if (pwm_percent_ == PWM_0_PERCENT)
      time_on_ = millis();
      
    if (percent < 15)
      pwm_percent_ = PWM_10_PERCENT;
    else if (percent < 25)
      pwm_percent_ = PWM_20_PERCENT;
    else if (percent < 35)
      pwm_percent_ = PWM_30_PERCENT;
    else if (percent < 45)
      pwm_percent_ = PWM_40_PERCENT;
    else if (percent < 55)
      pwm_percent_ = PWM_50_PERCENT;
    else if (percent < 65)
      pwm_percent_ = PWM_60_PERCENT;
    else if (percent < 75)
      pwm_percent_ = PWM_70_PERCENT;
    else if (percent < 85)
      pwm_percent_ = PWM_80_PERCENT;
    else if (percent < 95)
      pwm_percent_ = PWM_90_PERCENT;
    else if (percent <= 100)
      pwm_percent_ = PWM_100_PERCENT;
    else
    {
      Serial.print("SSRPump: pump pwm percent not valid! ");
      Serial.println(percent);
      pwm_percent_ = PWM_0_PERCENT;
      return;
    }
  }
}

PWM_Percent_t IRAM_ATTR SSRPump::getPWM()
{
  return pwm_percent_;
}

static void IRAM_ATTR timer_callback(void)
{
  static uint32_t pwm_period_counter_ = 0;  // elapsed periods
  
  if (instance == nullptr)
    return;
  
  if (!instance->isEnabled())
  {
    pwm_period_counter_ = 0;
    instance->off();
    return;
  }

  // called every 20ms == full sine period
  switch (instance->getPWM())
  {
    case PWM_0_PERCENT:
      instance->off();
      pwm_period_counter_ = 0;
      break;
    case PWM_10_PERCENT:
      if (pwm_period_counter_ == 0)
        instance->on();
      else
        instance->off();
      if (++pwm_period_counter_ >= 10)
        pwm_period_counter_ = 0;
      break;
    case PWM_20_PERCENT:
      if (pwm_period_counter_ == 0)
        instance->on();
      else
        instance->off();
      if (++pwm_period_counter_ >= 5)
        pwm_period_counter_ = 0;
      break;
    case PWM_30_PERCENT:
      if (pwm_period_counter_ == 0)
        instance->on();
      else
        instance->off();
      if (++pwm_period_counter_ >= 3)
        pwm_period_counter_ = 0;
      break;
    case PWM_40_PERCENT:
      if (pwm_period_counter_ == 0 || pwm_period_counter_ == 2)
        instance->on();
      else
        instance->off();
      if (++pwm_period_counter_ >= 5)
        pwm_period_counter_ = 0;
      break;
    case PWM_50_PERCENT:
      if (pwm_period_counter_ == 0)
        instance->on();
      else
        instance->off();
      if (++pwm_period_counter_ >= 2)
        pwm_period_counter_ = 0;
      break;
    case PWM_60_PERCENT:
      if (pwm_period_counter_ == 0 || pwm_period_counter_ == 1 || pwm_period_counter_ == 3)
        instance->on();
      else
        instance->off();
      if (++pwm_period_counter_ >= 5)
        pwm_period_counter_ = 0;
      break;
    case PWM_70_PERCENT:
      if (pwm_period_counter_ == 3 || pwm_period_counter_ == 6 || pwm_period_counter_ == 9)
        instance->off();
      else
        instance->on();
      if (++pwm_period_counter_ >= 10)
        pwm_period_counter_ = 0;
      break;
    case PWM_80_PERCENT:
      if (pwm_period_counter_ < 4)
        instance->on();
      else
        instance->off();
      if (++pwm_period_counter_ >= 5)
        pwm_period_counter_ = 0;
      break;
    case PWM_90_PERCENT:
      if (pwm_period_counter_ < 9)
        instance->on();
      else
        instance->off();
      if (++pwm_period_counter_ >= 10)
        pwm_period_counter_ = 0;
      break;
    case PWM_100_PERCENT:
      instance->on();
      pwm_period_counter_ = 0;
      break;
    default:
      instance->off();
  }
}
