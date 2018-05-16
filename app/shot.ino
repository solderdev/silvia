#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "Arduino.h"

typedef enum {
  SHOT_OFF = 0,
  SHOT_INIT_FILL,
  SHOT_RAMP,
  SHOT_PAUSE,
  SHOT_100PERCENT,
  
} SHOT_State_t;

static SHOT_State_t shot_state = SHOT_OFF;

static uint32_t shot_time_init_fill_ms = 0;  // ms - initial 100% pump duration
static uint32_t shot_time_ramp_ms = 0;       // ms - ramp duration from start-percent to stop-percent
static uint8_t shot_pump_start_percent = 0;  // % - start of ramp
static uint8_t shot_pump_stop_percent = 0;   // % - stop of ramp (also part of ramp)
static uint32_t shot_time_pause_ms = 0;      // ms - pause after ramp with 0% pump

static TimerHandle_t shot_timer = NULL;

static void shot_timer_cb(TimerHandle_t pxTimer);

void SHOT_setup(void)
{
  SHOT_stop(false, 0);

  if (shot_timer == NULL)
  {
    // shot timer - start must be called separately
    shot_timer = xTimerCreate("tmr_shot", pdMS_TO_TICKS(1), pdFALSE, NULL, shot_timer_cb);
    if (shot_timer == NULL)
      return; // error
  }
}

void SHOT_start(uint32_t init_fill_ms, uint32_t time_ramp_ms, uint32_t time_pause_ms, uint8_t pump_start_percent, uint8_t pump_stop_percent)
{
  if (init_fill_ms == 0 || init_fill_ms > 5000 ||
      time_ramp_ms > 10000 || time_pause_ms > 10000 ||
      pump_start_percent > 100 || pump_stop_percent > 100 || pump_start_percent > pump_stop_percent)
    return;
    
  if (shot_state == SHOT_OFF)
  {
    shot_time_init_fill_ms = init_fill_ms;
    shot_time_ramp_ms = time_ramp_ms;
    shot_time_pause_ms = time_pause_ms;
    shot_pump_start_percent = pump_start_percent;
    shot_pump_stop_percent = pump_stop_percent;
    
    shot_state = SHOT_INIT_FILL;
    SSRCTRL_set_state_valve(true);
    SSRCTRL_set_pwm_pump(100);
    // 100% pump for shot_time_init_fill seconds
    xTimerChangePeriod(shot_timer, pdMS_TO_TICKS(shot_time_init_fill_ms), portMAX_DELAY);
  }
}

void SHOT_stop(bool valve, uint8_t pump_percent)
{
  if (shot_state != SHOT_OFF)
  {
    shot_state = SHOT_OFF;
    xTimerStop(shot_timer, portMAX_DELAY);
    SSRCTRL_set_state_valve(valve);
    SSRCTRL_set_pwm_pump(pump_percent);
  }
}

// start: enable valve
// ramp up pump from shot_pump_percent_start to 100% in shot_time_ramp seconds
//   increment_duration = shot_time_ramp_ms / ((shot_pump_stop_percent - shot_pump_start_percent)/10)
//   
// continue with 100% until SHOT_stop()
static void shot_timer_cb(TimerHandle_t pxTimer)
{
  static uint8_t shot_current_ramp_percent = 0;
  (void)pxTimer;
  
  //Serial.println("Shot state " + String(shot_state));
  
  switch (shot_state)
  {
    case SHOT_OFF:
      SSRCTRL_set_state_valve(false);
      SSRCTRL_set_pwm_pump(0);
      xTimerStop(shot_timer, 0);
      break;
      
    case SHOT_INIT_FILL:
      // init fill is finished: go to ramp
      shot_state = SHOT_RAMP;
      shot_current_ramp_percent = shot_pump_start_percent;
      SSRCTRL_set_pwm_pump(shot_current_ramp_percent);
      xTimerChangePeriod(shot_timer, pdMS_TO_TICKS(shot_time_ramp_ms / ((shot_pump_stop_percent - shot_pump_start_percent)/10)), 0);
      break;
      
    case SHOT_RAMP:
      shot_current_ramp_percent += 10;
      if (shot_current_ramp_percent > shot_pump_stop_percent)
      {
        // ramp finished - continue with pause or 100%
        shot_current_ramp_percent = 0;
        if (shot_time_pause_ms > 0)
        {
          shot_state = SHOT_PAUSE;
          SSRCTRL_set_pwm_pump(0);
          xTimerChangePeriod(shot_timer, pdMS_TO_TICKS(shot_time_pause_ms), 0);
        }
        else
        {
          // go directly to 100%
          shot_state = SHOT_100PERCENT;
          SSRCTRL_set_pwm_pump(100);
        }
        break;
      }
      // continue with new ramp value
      SSRCTRL_set_pwm_pump(shot_current_ramp_percent);
      xTimerChangePeriod(shot_timer, pdMS_TO_TICKS(shot_time_ramp_ms / ((shot_pump_stop_percent - shot_pump_start_percent)/10)), 0);
      break;
      
    case SHOT_PAUSE:
      // pause done - continue with 100%
      shot_state = SHOT_100PERCENT;
      SSRCTRL_set_pwm_pump(100);
      break;
      
    case SHOT_100PERCENT:
      break;
      
    default:
      Serial.println("Shot ERROR state");
      break;
  }
}



