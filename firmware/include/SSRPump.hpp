#pragma once

#include <Arduino.h>
#include "SSR.hpp"

typedef enum PWM_Percent {
  PWM_100_PERCENT = 100,
  PWM_90_PERCENT = 90,
  PWM_80_PERCENT = 80,
  PWM_70_PERCENT = 70,
  PWM_60_PERCENT = 60,
  PWM_50_PERCENT = 50,
  PWM_40_PERCENT = 40,
  PWM_30_PERCENT = 30,
  PWM_20_PERCENT = 20,
  PWM_10_PERCENT = 10,
  PWM_0_PERCENT = 0,
} PWM_Percent_t;

class SSRPump: public SSR
{
public:
  SSRPump(uint8_t ctrl_pin, int32_t timer_id, uint32_t timer_period_us);
  SSRPump(SSRPump const&) = delete;
  void operator=(SSRPump const&)  = delete;

  static SSRPump* getInstance();

  void setPWM(uint8_t percent);
  PWM_Percent_t getPWM();

private:
  hw_timer_t *timer_pwm_;
  volatile PWM_Percent_t pwm_percent_;
  uint32_t time_on_;
};
