#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"


#define DEBOUNCE_INTERVAL_DD   1   // [ms]
#define DEBOUNCE_COUNT_DD      10
#define DEBOUNCE_INTERVAL_BTN  11  // [ms]
#define DEBOUNCE_COUNT_BTN     4

#define PIN_DD_BUTTON       5  // TODO - check
#define PIN_DD_DT          18  // TODO - check
#define PIN_DD_CLK         19  // TODO - check
#define PIN_POWER_BUTTON   23
#define PIN_SWITCH_COFFEE  17
#define PIN_SWITCH_WATER   16
#define PIN_SWITCH_STEAM    4

#define ROT_DD_1   ((digitalRead(PIN_DD_DT))?1:0)
#define ROT_DD_2   ((digitalRead(PIN_DD_CLK))?1:0)
#define ROT_DD_BTN ((digitalRead(PIN_DD_BUTTON))?1:0)
#define BTN_POWER  ((digitalRead(PIN_POWER_BUTTON))?0:1)


static TimerHandle_t timer_debounce_dd = NULL;
static TimerHandle_t timer_debounce_btn = NULL;

static uint8_t cw_count = 0;
static uint8_t ccw_count = 0;

static uint8_t btn_power_count = 0;  // limited to max. 1


static void dd_timer_cb(TimerHandle_t pxTimer);
static void btn_timer_cb(TimerHandle_t pxTimer);


uint8_t BTN_setup(void)
{
  pinMode(PIN_DD_BUTTON, INPUT);
  pinMode(PIN_DD_DT, INPUT);
  pinMode(PIN_DD_CLK, INPUT);
  pinMode(PIN_POWER_BUTTON, INPUT);
  pinMode(PIN_SWITCH_COFFEE, INPUT);
  pinMode(PIN_SWITCH_WATER, INPUT);
  pinMode(PIN_SWITCH_STEAM, INPUT);

  if (timer_debounce_dd == NULL)
    timer_debounce_dd = xTimerCreate("tmr_dd", pdMS_TO_TICKS(DEBOUNCE_INTERVAL_DD), pdTRUE, NULL, dd_timer_cb);
  if (timer_debounce_dd == NULL)
    return 1; // error
  xTimerStart(timer_debounce_dd, portMAX_DELAY);

  if (timer_debounce_btn == NULL)
    timer_debounce_btn = xTimerCreate("tmr_btn", pdMS_TO_TICKS(DEBOUNCE_INTERVAL_BTN), pdTRUE, NULL, btn_timer_cb);
  if (timer_debounce_btn == NULL)
    return 1; // error
  xTimerStart(timer_debounce_btn, portMAX_DELAY);

  cw_count = 0;
  ccw_count = 0;
  btn_power_count = 0;
  
  return 0;
}

boolean BTN_getDDcw(void)
{
  boolean retval;
  if (cw_count > 0)
  {
    retval = true;
    cw_count--;
  }
  else
    retval = false;
  return retval;
}

boolean BTN_getDDccw(void)
{
  boolean retval;
  if (ccw_count > 0)
  {
    retval = true;
    ccw_count--;
  }
  else
    retval = false;
  return retval;
}

static void dd_timer_cb(TimerHandle_t pxTimer)
{
  static int8_t dd1_debounce = 0;
  static int8_t dd2_debounce = 0;
  static int8_t idle_debounce = 0;
  static uint8_t first_falling_edge;
  (void)pxTimer;

  // get pin states
  if (ROT_DD_1)
    // increase counter up to DEBOUNCE_COUNT_DD if pin is high
    dd1_debounce = (dd1_debounce + 1 > DEBOUNCE_COUNT_DD) ? DEBOUNCE_COUNT_DD : dd1_debounce + 1;
  else
  {
    // if pin gets low by turning the knob, save turning direction
    if (idle_debounce == DEBOUNCE_COUNT_DD)
      first_falling_edge = 1;  // pin 1 received first falling edge after idle high level
    dd1_debounce = 0;
  }

  // same as DD1
  if (ROT_DD_2)
    dd2_debounce = (dd2_debounce + 1 > DEBOUNCE_COUNT_DD) ? DEBOUNCE_COUNT_DD : dd2_debounce + 1;
  else
  {
    if (idle_debounce == DEBOUNCE_COUNT_DD)
      first_falling_edge = 2;  // pin 2 received first falling edge after idle high level
    dd2_debounce = 0;
  }

  // wait until both pins are debounced in the idle position
  if (dd1_debounce == DEBOUNCE_COUNT_DD && dd2_debounce == DEBOUNCE_COUNT_DD)
    idle_debounce = (idle_debounce + 1 > DEBOUNCE_COUNT_DD) ? DEBOUNCE_COUNT_DD : idle_debounce + 1;
  else
    idle_debounce = 0;

  // then decide which edge triggered the event
  if (idle_debounce == DEBOUNCE_COUNT_DD)
  {
    if (first_falling_edge == 1)
      ccw_count++;
    else if (first_falling_edge == 2)
      cw_count++;
    // reset saved event - wait for new "first" low edge
    first_falling_edge = 0;
  }
}

static void btn_timer_cb(TimerHandle_t pxTimer)
{
  (void)pxTimer;

  // TODO

  if (btn_power_count > 1)
    btn_power_count = 1;
}
