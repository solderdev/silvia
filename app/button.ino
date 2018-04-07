#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"


#define DEBOUNCE_INTERVAL  1  // [ms]
#define DEBOUNCE_COUNT     10

#define ROT_DD_1 ((digitalRead(DTPin))?1:0)
#define ROT_DD_2 ((digitalRead(CLKPin))?1:0)
#define ROT_DD_B digitalRead(buttonPin)


static const int buttonPin = 32;
static const int DTPin = 34;
static const int CLKPin = 35;

static TimerHandle_t timer_debounce = NULL;
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

static int8_t dd1_debounce = 0;
static int8_t dd2_debounce = 0;
static int8_t idle_debounce = 0;
static uint8_t first_falling_edge;
static uint8_t cw_count = 0;
static uint8_t ccw_count = 0;


static void btn_timer_cb(TimerHandle_t pxTimer);


uint8_t BTN_setup(void)
{
  pinMode(buttonPin, INPUT);
  pinMode(DTPin, INPUT);
  pinMode(CLKPin, INPUT);
  
  timer_debounce = xTimerCreate("tmr_btn", pdMS_TO_TICKS(DEBOUNCE_INTERVAL), pdTRUE, NULL, btn_timer_cb);
  if (timer_debounce == NULL)
    return 1; // error
  xTimerStart(timer_debounce, portMAX_DELAY);

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

static void btn_timer_cb(TimerHandle_t pxTimer)
{
  (void)pxTimer;

  // get pin states
  if (ROT_DD_1)
    // increase counter up to DEBOUNCE_COUNT if pin is high
    dd1_debounce = (dd1_debounce + 1 > DEBOUNCE_COUNT) ? DEBOUNCE_COUNT : dd1_debounce + 1;
  else
  {
    // if pin gets low by turning the knob, save turning direction
    if (idle_debounce == DEBOUNCE_COUNT)
      first_falling_edge = 1;  // pin 1 received first falling edge after idle high level
    dd1_debounce = 0;
  }

  // same as DD1
  if (ROT_DD_2)
    dd2_debounce = (dd2_debounce + 1 > DEBOUNCE_COUNT) ? DEBOUNCE_COUNT : dd2_debounce + 1;
  else
  {
    if (idle_debounce == DEBOUNCE_COUNT)
      first_falling_edge = 2;  // pin 2 received first falling edge after idle high level
    dd2_debounce = 0;
  }

  // wait until both pins are debounced in the idle position
  if (dd1_debounce == DEBOUNCE_COUNT && dd2_debounce == DEBOUNCE_COUNT)
    idle_debounce = (idle_debounce + 1 > DEBOUNCE_COUNT) ? DEBOUNCE_COUNT : idle_debounce + 1;
  else
    idle_debounce = 0;

  // then decide which edge triggered the event
  if (idle_debounce == DEBOUNCE_COUNT)
  {
    if (first_falling_edge == 1)
      ccw_count++;
    else if (first_falling_edge == 2)
      cw_count++;
    // reset saved event - wait for new "first" low edge
    first_falling_edge = 0;
  }
}
