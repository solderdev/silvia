#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"


#define PIN_SSR_PUMP    26  // TODO - check
#define PIN_SSR_HEATER  27  // TODO - check
#define PIN_SSR_VALVE   33  // TODO - check

#define HEATER_MAX_ON_SEC  50u   // [s]
#define HEATER_COOL_SEC    100u  // [s]
#define PUMP_MAX_ON_SEC    45u   // [s]
#define PUMP_COOL_SEC      50u   // [s]
#define VALVE_MAX_ON_SEC   PUMP_MAX_ON_SEC  // [s]
#define VALVE_COOL_SEC     PUMP_COOL_SEC    // [s]

#define SSR_ON   true
#define SSR_OFF  false

#define SSR_HEATER_OFF()  do{digitalWrite(PIN_SSR_HEATER, LOW); heater_ctrl.current_state = SSR_OFF; heater_ctrl.time_current_state = millis();}while(0)
#define SSR_HEATER_ON()   do{digitalWrite(PIN_SSR_HEATER, HIGH); heater_ctrl.current_state = SSR_ON; heater_ctrl.time_current_state = millis();}while(0)
#define SSR_PUMP_OFF()    do{digitalWrite(PIN_SSR_PUMP, LOW); pump_ctrl.current_state = SSR_OFF; pump_ctrl.time_current_state = millis();}while(0)
#define SSR_PUMP_ON()     do{digitalWrite(PIN_SSR_PUMP, HIGH); pump_ctrl.current_state = SSR_ON; pump_ctrl.time_current_state = millis();}while(0)
#define SSR_VALVE_OFF()   do{digitalWrite(PIN_SSR_VALVE, LOW); valve_ctrl.current_state = SSR_OFF; valve_ctrl.time_current_state = millis();}while(0)
#define SSR_VALVE_ON()    do{digitalWrite(PIN_SSR_VALVE, HIGH); valve_ctrl.current_state = SSR_ON; valve_ctrl.time_current_state = millis();}while(0)


typedef struct SSR_Control {
  TimerHandle_t timer_pwm;
  uint32_t time_current_state;
  bool target_state;
  bool current_state;
} SSR_Control_t;


static bool enabled = false;
static TimerHandle_t timer_safety = NULL;
static SSR_Control_t heater_ctrl;
static SSR_Control_t pump_ctrl;
static SSR_Control_t valve_ctrl;


static void heater_timer_cb(TimerHandle_t pxTimer);
static void pump_timer_cb(TimerHandle_t pxTimer);
static void safety_timer_cb(TimerHandle_t pxTimer);


uint8_t SSRCTRL_setup(void)
{
  if (enabled == true)
    return 0;

  // arduino doesn't like {0} on declaration -.-
  memset(&heater_ctrl, 0, sizeof(heater_ctrl));
  memset(&pump_ctrl, 0, sizeof(pump_ctrl));
  memset(&valve_ctrl, 0, sizeof(valve_ctrl));

  SSRCTRL_on();
  
  // init PWM control timer
  // heater: 50Hz -> whole sine: 20ms, half sine: 10ms
  heater_ctrl.timer_pwm = xTimerCreate("tmr_ssr_h", pdMS_TO_TICKS(10), pdTRUE, NULL, heater_timer_cb);
  // pump: 50Hz -> whole sine: 20ms - half sine not possible due to internal diode
  pump_ctrl.timer_pwm = xTimerCreate("tmr_ssr_p", pdMS_TO_TICKS(20), pdTRUE, NULL, pump_timer_cb);
  if (heater_ctrl.timer_pwm == NULL || pump_ctrl.timer_pwm == NULL)
    return 1; // error

  // init safety timer
  timer_safety = xTimerCreate("tmr_safety", pdMS_TO_TICKS(997), pdTRUE, NULL, safety_timer_cb);
  if (timer_safety == NULL)
    return 1; // error

  return 0;
}

static void heater_timer_cb(TimerHandle_t pxTimer)
{
  (void)pxTimer;
  if (enabled != true)
    return;

  // TODO
}

static void pump_timer_cb(TimerHandle_t pxTimer)
{
  (void)pxTimer;
  if (enabled != true)
    return;

  // TODO
}

static void safety_timer_cb(TimerHandle_t pxTimer)
{
  (void)pxTimer;

  // make sure heater is on for max. HEATER_MAX_ON_SEC and can cool off for HEATER_COOL_SEC
  if (heater_ctrl.target_state == heater_ctrl.current_state)
  {
    // check if on for too long
    if (heater_ctrl.target_state == SSR_ON && (millis() - heater_ctrl.time_current_state > HEATER_MAX_ON_SEC*1000))
    {
      Serial.println("SSRctrl: heater on for too long!");
      SSR_HEATER_OFF();
    }
  }
  else
  {
    // must have been switched off for safety
    if (heater_ctrl.target_state == SSR_ON && (millis() - heater_ctrl.time_current_state > HEATER_COOL_SEC*1000))
    {
      Serial.println("SSRctrl: heater re-activated after cooldown!");
      SSR_HEATER_ON();
    }
  }

  // switch off heater in case boiler is too hot
  // gets re-enabled above after HEATER_COOL_SEC
  if (SENSORS_get_temp_boiler_side() > PID_MAX_TEMP || SENSORS_get_temp_boiler_top() > PID_MAX_TEMP)
  {
    Serial.println("SSRctrl: boiler too hot!");
    SSR_HEATER_OFF();
  }

  // make sure pump is on for max. PUMP_MAX_ON_SEC and can cool off for PUMP_COOL_SEC
  if (pump_ctrl.target_state == pump_ctrl.current_state)
  {
    // check if on for too long
    if (pump_ctrl.target_state == SSR_ON && (millis() - pump_ctrl.time_current_state > PUMP_MAX_ON_SEC*1000))
    {
      Serial.println("SSRctrl: pump on for too long!");
      SSR_PUMP_OFF();
    }
  }
  else
  {
    // must have been switched off for safety
    if (pump_ctrl.target_state == SSR_ON && (millis() - pump_ctrl.time_current_state > PUMP_COOL_SEC*1000))
    {
      Serial.println("SSRctrl: pump re-activated after cooldown!");
      SSR_PUMP_ON();
    }
  }

  // make sure valve is on for max. VALVE_MAX_ON_SEC and can cool off for VALVE_COOL_SEC
  if (valve_ctrl.target_state == valve_ctrl.current_state)
  {
    // check if on for too long
    if (valve_ctrl.target_state == SSR_ON && (millis() - valve_ctrl.time_current_state > VALVE_MAX_ON_SEC*1000))
    {
      Serial.println("SSRctrl: valve on for too long!");
      SSR_VALVE_OFF();
    }
  }
  else
  {
    // must have been switched off for safety
    if (valve_ctrl.target_state == SSR_ON && (millis() - valve_ctrl.time_current_state > VALVE_COOL_SEC*1000))
    {
      Serial.println("SSRctrl: valve re-activated after cooldown!");
      SSR_VALVE_ON();
    }
  }
}

void SSRCTRL_on(void)
{
  // enable controls in general

  SSR_HEATER_OFF();
  SSR_PUMP_OFF();
  SSR_VALVE_OFF();
  pinMode(PIN_SSR_PUMP, OUTPUT);
  pinMode(PIN_SSR_HEATER, OUTPUT);
  pinMode(PIN_SSR_VALVE, OUTPUT);
  heater_ctrl.target_state = SSR_OFF;
  pump_ctrl.target_state = SSR_OFF;
  valve_ctrl.target_state = SSR_OFF;
  
  enabled = true;
}

void SSRCTRL_off(void)
{
  // disable controls in general

  // pins as input
  SSR_HEATER_OFF();
  SSR_PUMP_OFF();
  SSR_VALVE_OFF();
  pinMode(PIN_SSR_PUMP, INPUT);
  pinMode(PIN_SSR_HEATER, INPUT);
  pinMode(PIN_SSR_VALVE, INPUT);
  heater_ctrl.target_state = SSR_OFF;
  pump_ctrl.target_state = SSR_OFF;
  valve_ctrl.target_state = SSR_OFF;

  enabled = false;
}

void SSRCTRL_set_pwm_heater(uint8_t percent)
{
  // TODO
}

void SSRCTRL_set_pwm_pump(uint8_t percent)
{
  // TODO
}

