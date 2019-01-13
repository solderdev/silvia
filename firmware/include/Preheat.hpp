#pragma once

#include <Arduino.h>

typedef enum {
  PREHEAT_OFF = 0,
  PREHEAT_100PERCENT,
  PREHEAT_STOPPUMP,
  PREHEAT_PAUSE
} PREHEAT_State_t;

class WaterControl;

class Preheat
{
public:
  Preheat(WaterControl *water_control);
  void start(uint32_t time_pump_ms, uint32_t time_stop_pump_ms, uint32_t time_pause_ms);
  void stop(uint8_t pump_percent, bool valve);

private:
  WaterControl *water_control_;
  PREHEAT_State_t state_;
  TimerHandle_t timer_;

  uint32_t time_pump_ms_;       // ms - 100% pump duration with valve on
  uint32_t time_stop_pump_ms_;  // ms - pump on, valve off duration
  uint32_t time_pause_ms_;      // ms - pause between cycles

  portMUX_TYPE mux_;
  void timer_cb();
  static void timer_cb_wrapper(TimerHandle_t arg);
};
