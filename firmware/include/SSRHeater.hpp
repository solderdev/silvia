#pragma once

#include <Arduino.h>
#include "SSR.hpp"

class SSRHeater: public SSR
{
public:
  SSRHeater(uint8_t ctrl_pin, int32_t timer_id, uint32_t timer_period_us);
  SSRHeater(SSRHeater const&) = delete;
  void operator=(SSRHeater const&)  = delete;
  static SSRHeater* getInstance();
  void sync();
  void setPWM(uint8_t percent);
  uint8_t getPWM();

private:
  hw_timer_t *timer_pwm_;
  uint8_t pwm_percent_;
};
