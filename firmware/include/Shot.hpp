#pragma once

#include <Arduino.h>

class WaterControl;

class Shot
{
public:
  Shot(WaterControl *water_control);
  void start(uint32_t init_fill_ms, uint32_t time_ramp_ms, uint32_t time_pause_ms, uint8_t pump_start_percent, uint8_t pump_stop_percent);
  void stop(uint8_t pump_percent, bool valve);
  uint32_t getShotTime();

private:
  WaterControl *water_control_;
  TimerHandle_t timer_;
  
  uint32_t time_init_fill_ms_;  // ms - initial 100% pump duration
  uint32_t time_ramp_ms_;       // ms - ramp duration from start-percent to stop-percent
  uint32_t time_pause_ms_;      // ms - pause after ramp with 0% pump
  uint8_t pump_start_percent_;  // % - start of ramp
  uint8_t pump_stop_percent_;   // % - stop of ramp (also part of ramp)
  uint8_t current_ramp_percent_;

  uint32_t start_time_;         // time-ms - start of current shot
  uint32_t stop_time_;          // time-ms - stop of current shot

  bool active_;                 // shot is currently active

  // commands for task to process
  enum SHOT_CMD {
    CMD_STOP = 0,
    CMD_START,
    CMD_RAMP,
    CMD_PAUSE,
    CMD_100PERCENT,
  };
  // queue sending commands to task
  static constexpr uint8_t cmd_queue_size_ = 5;
  static constexpr uint8_t cmd_queue_item_size_ = sizeof(uint32_t);
  QueueHandle_t cmd_queue_;

  static void task_wrapper(void *arg);
  void task();
  TaskHandle_t task_handle_;

  // current timer infos tell which command needs to be executed next by timer
  typedef struct TimerInfos {
    Shot *shot;
    uint32_t cmd;
  } TimerInfos_t;
  TimerInfos_t timer_infos_;

  void timer_cb(uint32_t cmd);
  static void timer_cb_wrapper(TimerHandle_t arg);
};
