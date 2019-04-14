#pragma once

#include <Arduino.h>

class WaterControl;


typedef enum {
  SHOT_OFF = 0,
  SHOT_INIT_FILL,
  SHOT_RAMP,
  SHOT_PAUSE,
  SHOT_100PERCENT,
  SHOT_MANUAL
} SHOT_State_t;


class Shot
{
public:
  Shot(WaterControl *water_control);
  void start(uint32_t init_fill_ms, uint32_t time_ramp_ms, uint32_t time_pause_ms, uint8_t pump_start_percent, uint8_t pump_stop_percent);
  void stop(uint8_t pump_percent, bool valve);
  uint32_t getShotTime();

private:
  WaterControl *water_control_;
  SHOT_State_t state_;
  TimerHandle_t timer_;
  
  uint32_t time_init_fill_ms_;  // ms - initial 100% pump duration
  uint32_t time_ramp_ms_;       // ms - ramp duration from start-percent to stop-percent
  uint32_t time_pause_ms_;      // ms - pause after ramp with 0% pump
  uint8_t pump_start_percent_;  // % - start of ramp
  uint8_t pump_stop_percent_;   // % - stop of ramp (also part of ramp)
  uint8_t current_ramp_percent_;

  uint32_t start_time_;         // time-ms - start of current shot
  uint32_t stop_time_;          // time-ms - stop of current shot

  portMUX_TYPE mux_;
  void timer_cb();
  static void timer_cb_wrapper(TimerHandle_t arg);
};
