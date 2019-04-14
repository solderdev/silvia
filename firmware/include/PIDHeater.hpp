#pragma once

#include <Arduino.h>
#include "coffee_config.hpp"

class WaterControl;
class SSRHeater;


typedef enum {
  PID_MODE_WATER,
  PID_MODE_STEAM
} PID_Mode_t;


#define PID_MIN_TEMP   10.0f  // deg-C - minimum allowed temperature
#define PID_MAX_TEMP  139.0f  // deg-C - maximum allowed temperature


class PIDHeater
{
public:
  // quite good: P+: 45  P-: 100  I: 1.2  D: 0
  PIDHeater(WaterControl *water_control, float p_pos = PID_P_POS, float p_neg = PID_P_NEG, float i = PID_I, float d = PID_D, uint32_t ts_ms = PID_TS);
  void start();
  void stop();
  void overrideOutput(float u_override, int8_t count);
  void setTarget(float temp, PID_Mode_t mode);
  float getTarget() {return target_;};
  float getPShare() {return p_share_;};
  float getIShare() {return i_share_;};
  float getDShare() {return d_share_;};
  float getUncorrectedOutput() {return u_;};

private:
  WaterControl *water_control_;
  SSRHeater *heater_;
  float kPpos_;  // positive contribution, if PV_old > PV
  float kPneg_;  // negative contribution, if PV_old < PV
  float kI_;
  float kD_;
  uint32_t ts_;  // ms - update interval
  float u_;  // uncorrected output value
  float p_share_, i_share_, d_share_;  // influences of the controller parts
  float u1_;  // last output value
  float pv1_;  // last process value (temperature)
  float pv2_;  // second to last process value (temperature)
  float target_;  // deg-C - target boiler temperature
  PID_Mode_t mode_;
  float u_override_;  // override next PID iteration with this value if positive
  int8_t u_override_cnt_;  // counter for how many PID cycles the override should be in place
  bool enabled_;

  SemaphoreHandle_t sem_update_;
  TimerHandle_t timer_update_;
  TaskHandle_t task_handle_;

  static void timer_cb_wrapper(TimerHandle_t arg);
  void timer_cb();
  static void task_wrapper(void *arg);
  void task();
};
