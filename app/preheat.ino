#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "Arduino.h"

typedef enum {
  PREHEAT_OFF = 0,
  PREHEAT_100PERCENT,
  PREHEAT_STOPPUMP,
  PREHEAT_PAUSE,
  
} PREHEAT_State_t;

static PREHEAT_State_t preheat_state = PREHEAT_OFF;

static uint32_t preheat_time_pump_ms = 0;       // ms - 100% pump duration with valve on
static uint32_t preheat_time_stop_pump_ms = 0;  // ms - pump on, valve off duration
static uint32_t preheat_time_pause_ms = 0;      // ms - pause

static TimerHandle_t preheat_timer = NULL;

static void preheat_timer_cb(TimerHandle_t pxTimer);

void PREHEAT_setup(void)
{
  PREHEAT_stop(false, 0);

  if (preheat_timer == NULL)
  {
    // preheat timer - start must be called separately
    preheat_timer = xTimerCreate("tmr_preheat", pdMS_TO_TICKS(1), pdFALSE, NULL, preheat_timer_cb);
    if (preheat_timer == NULL)
      return; // error
  }
}

void PREHEAT_start(uint32_t time_pump_ms, uint32_t time_stop_pump_ms, uint32_t time_pause_ms)
{
  if (time_pump_ms < 100 || time_pump_ms > 5000 ||
      time_stop_pump_ms > 2000 || 
      time_pause_ms < 4000 || time_pause_ms > 50000)
    return;
    
  if (preheat_state == PREHEAT_OFF)
  {
    preheat_time_pump_ms = time_pump_ms;
    preheat_time_stop_pump_ms = time_stop_pump_ms;
    preheat_time_pause_ms = time_pause_ms;

    // call timer-cb once to start loop
    preheat_state = PREHEAT_100PERCENT;
    preheat_timer_cb(NULL);
  }
}

void PREHEAT_stop(bool valve, uint8_t pump_percent)
{
  if (preheat_state != PREHEAT_OFF)
  {
    xTimerStop(preheat_timer, portMAX_DELAY);
    preheat_state = PREHEAT_OFF;
    SSRCTRL_set_state_valve(valve);
    SSRCTRL_set_pwm_pump(pump_percent);
  }
}

// enable valve + 100% pump
// wait preheat_time_pump_ms
// disable valve
// wait preheat_time_stop_pump_ms
// 0% pump
// wait preheat_time_pause_ms
static void preheat_timer_cb(TimerHandle_t pxTimer)
{
  (void)pxTimer;
  
  switch (preheat_state)
  {
    case PREHEAT_OFF:
      SSRCTRL_set_state_valve(false);
      SSRCTRL_set_pwm_pump(0);
      xTimerStop(preheat_timer, 0);
      break;
      
    case PREHEAT_100PERCENT:
      preheat_state = PREHEAT_STOPPUMP;
      SSRCTRL_set_state_valve(true);
      SSRCTRL_set_pwm_pump(100);
      xTimerChangePeriod(preheat_timer, pdMS_TO_TICKS(preheat_time_pump_ms), 0);
      break;
      
    case PREHEAT_STOPPUMP:
      preheat_state = PREHEAT_PAUSE;
      SSRCTRL_set_state_valve(false);
      xTimerChangePeriod(preheat_timer, pdMS_TO_TICKS(preheat_time_stop_pump_ms), 0);
      break;
      
    case PREHEAT_PAUSE:
      preheat_state = PREHEAT_100PERCENT;
      SSRCTRL_set_state_valve(false);
      SSRCTRL_set_pwm_pump(0);
      xTimerChangePeriod(preheat_timer, pdMS_TO_TICKS(preheat_time_pause_ms), 0);
      break;
      
    default:
      Serial.println("Preheat ERROR state");
      break;
  }
}

