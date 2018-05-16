#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"


#define PIN_SSR_PUMP    27
#define PIN_SSR_HEATER  26
#define PIN_SSR_VALVE   33

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

typedef enum PWM_Pump {
  PWM_PUMP_100_PERCENT,
  PWM_PUMP_90_PERCENT,
  PWM_PUMP_80_PERCENT,
  PWM_PUMP_70_PERCENT,
  PWM_PUMP_60_PERCENT,
  PWM_PUMP_50_PERCENT,
  PWM_PUMP_40_PERCENT,
  PWM_PUMP_30_PERCENT,
  PWM_PUMP_20_PERCENT,
  PWM_PUMP_10_PERCENT,
  PWM_PUMP_0_PERCENT,
} PWM_Pump_t;


static bool enabled = false;
static TimerHandle_t timer_safety = NULL;
static SSR_Control_t heater_ctrl;
static SSR_Control_t pump_ctrl;
static SSR_Control_t valve_ctrl;
static PWM_Pump_t pwm_percent_pump = PWM_PUMP_0_PERCENT;
static uint8_t pwm_percent_heater[10] = {0};
static uint8_t pwm_percent_heater_int = 0;


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
  
  pwm_percent_pump = PWM_PUMP_0_PERCENT;
  memset(pwm_percent_heater, 0, sizeof(pwm_percent_heater));

  SSRCTRL_off();
  
  // init PWM control timer
  // heater: 50Hz -> whole sine: 20ms, half sine: 10ms
  heater_ctrl.timer_pwm = xTimerCreate("tmr_ssr_h", pdMS_TO_TICKS(10), pdTRUE, NULL, heater_timer_cb);
  // pump: 50Hz -> whole sine: 20ms - half sine not possible due to internal diode
  pump_ctrl.timer_pwm = xTimerCreate("tmr_ssr_p", pdMS_TO_TICKS(20), pdTRUE, NULL, pump_timer_cb);
  if (heater_ctrl.timer_pwm == NULL || pump_ctrl.timer_pwm == NULL)
    return 1; // error
  xTimerStart(heater_ctrl.timer_pwm, portMAX_DELAY);
  xTimerStart(pump_ctrl.timer_pwm, portMAX_DELAY);

  // init safety timer
  timer_safety = xTimerCreate("tmr_safety", pdMS_TO_TICKS(997), pdTRUE, NULL, safety_timer_cb);
  if (timer_safety == NULL)
    return 1; // error
  xTimerStart(timer_safety, portMAX_DELAY);

  return 0;
}

static void heater_timer_cb(TimerHandle_t pxTimer)
{
  static uint32_t period_counter = 0;  // 0 to 99 elapsed periods
//  static uint32_t periods_on = 0;
  (void)pxTimer;
  
  if (enabled != true)
    return;
  
  if (pwm_percent_heater[period_counter/10] > period_counter % 10)
  {
    SSR_HEATER_ON();
//    periods_on++;
  }
  else
    SSR_HEATER_OFF();
  
  if (++period_counter >= 100)
  {
    period_counter = 0;
//    Serial.println("periods on: " + String(periods_on));
//    periods_on = 0;
  }
}

static void pump_timer_cb(TimerHandle_t pxTimer)
{
  static uint32_t period_counter = 0;  // elapsed periods
  (void)pxTimer;
  
  if (enabled != true)
    return;

  // called every 20ms == full sine period
  switch (pwm_percent_pump)
  {
    case PWM_PUMP_0_PERCENT:
      SSR_PUMP_OFF();
      period_counter = 0;
      break;
    case PWM_PUMP_10_PERCENT:
      if (period_counter == 0)
        SSR_PUMP_ON();
      else
        SSR_PUMP_OFF();
      if (++period_counter >= 10)
        period_counter = 0;
      break;
    case PWM_PUMP_20_PERCENT:
      if (period_counter == 0)
        SSR_PUMP_ON();
      else
        SSR_PUMP_OFF();
      if (++period_counter >= 5)
        period_counter = 0;
      break;
    case PWM_PUMP_30_PERCENT:
      if (period_counter == 0)
        SSR_PUMP_ON();
      else
        SSR_PUMP_OFF();
      if (++period_counter >= 3)
        period_counter = 0;
      break;
    case PWM_PUMP_40_PERCENT:
      if (period_counter == 0 || period_counter == 2)
        SSR_PUMP_ON();
      else
        SSR_PUMP_OFF();
      if (++period_counter >= 5)
        period_counter = 0;
      break;
    case PWM_PUMP_50_PERCENT:
      if (period_counter == 0)
        SSR_PUMP_ON();
      else
        SSR_PUMP_OFF();
      if (++period_counter >= 2)
        period_counter = 0;
      break;
    case PWM_PUMP_60_PERCENT:
      if (period_counter == 0 || period_counter == 1 || period_counter == 3)
        SSR_PUMP_ON();
      else
        SSR_PUMP_OFF();
      if (++period_counter >= 5)
        period_counter = 0;
      break;
    case PWM_PUMP_70_PERCENT:
      if (period_counter == 0 || period_counter == 1 || period_counter == 2 || period_counter == 4 || period_counter == 5 || period_counter == 7 || period_counter == 8)
        SSR_PUMP_ON();
      else
        SSR_PUMP_OFF();
      if (++period_counter >= 10)
        period_counter = 0;
      break;
    case PWM_PUMP_80_PERCENT:
      if (period_counter < 4)
        SSR_PUMP_ON();
      else
        SSR_PUMP_OFF();
      if (++period_counter >= 5)
        period_counter = 0;
      break;
    case PWM_PUMP_90_PERCENT:
      if (period_counter < 9)
        SSR_PUMP_ON();
      else
        SSR_PUMP_OFF();
      if (++period_counter >= 10)
        period_counter = 0;
      break;
    case PWM_PUMP_100_PERCENT:
      SSR_PUMP_ON();
      period_counter = 0;
      break;
    default:
      SSR_PUMP_OFF();
      Serial.println("SSRctrl: illegal pump PWM!");
  }
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
    Serial.println("SSRctrl: boiler too hot! Side: " + String(SENSORS_get_temp_boiler_side()) + 
                   " Top: " + String(SENSORS_get_temp_boiler_top()));
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
  Serial.println("SSR ctrl ON!");
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
  Serial.println("SSR ctrl OFF!");
}

bool SSRCTRL_getState(void)
{
  return enabled;
}

void SSRCTRL_set_pwm_heater(uint8_t percent)
{ 
  if (percent > 100)
  {
    percent = 0;
    Serial.println("SSRctrl: heater percent > 100!");
  }
  
  if (percent == 100)
  {
    memset(pwm_percent_heater, 0xFF, sizeof(pwm_percent_heater));  // set array
  }
  else
  {
    memset(pwm_percent_heater, 0, sizeof(pwm_percent_heater));  // reset array
    
    // fill array in each 10-percent field (tens) with 1s according to number of every 10%
    uint8_t max_ones = percent/10;
    for (uint8_t tens = 0; tens < 10; tens++)
    {
      for (uint8_t ones = 0; ones < max_ones; ones++)
        pwm_percent_heater[tens] += 1;
    }
    // now handle remaining 0% to 9% on the first digit of the overall percent value
    uint8_t max_tens = percent - (percent/10) * 10;
    for (uint8_t tens = 0; tens < max_tens; tens++)
      pwm_percent_heater[tens] += 1;
  }
  pwm_percent_heater_int = percent;
  
//  Serial.println("Heater percent: " + String(percent));
//  for (uint8_t i = 0; i < 10; i++)
//    Serial.print(String(pwm_percent_heater[i]) + " ");
//  Serial.println("");
}

uint8_t SSRCTRL_get_pwm_heater(void)
{
  return pwm_percent_heater_int;
}

void SSRCTRL_set_pwm_pump(uint8_t percent)
{
  if (percent < 5)
    pwm_percent_pump = PWM_PUMP_0_PERCENT;
  else if (percent < 15)
    pwm_percent_pump = PWM_PUMP_10_PERCENT;
  else if (percent < 25)
    pwm_percent_pump = PWM_PUMP_20_PERCENT;
  else if (percent < 35)
    pwm_percent_pump = PWM_PUMP_30_PERCENT;
  else if (percent < 45)
    pwm_percent_pump = PWM_PUMP_40_PERCENT;
  else if (percent < 55)
    pwm_percent_pump = PWM_PUMP_50_PERCENT;
  else if (percent < 65)
    pwm_percent_pump = PWM_PUMP_60_PERCENT;
  else if (percent < 75)
    pwm_percent_pump = PWM_PUMP_70_PERCENT;
  else if (percent < 85)
    pwm_percent_pump = PWM_PUMP_80_PERCENT;
  else if (percent < 95)
    pwm_percent_pump = PWM_PUMP_90_PERCENT;
  else if (percent <= 100)
    pwm_percent_pump = PWM_PUMP_100_PERCENT;
  else
  {
    Serial.println("SSRctrl: pump pwm percent not valid!");
    pwm_percent_pump = PWM_PUMP_0_PERCENT;
  }
}

uint8_t SSRCTRL_get_pwm_pump(void)
{
  if (pwm_percent_pump == PWM_PUMP_0_PERCENT)
    return 0;
  else if (pwm_percent_pump == PWM_PUMP_10_PERCENT)
    return 10;
  else if (pwm_percent_pump == PWM_PUMP_20_PERCENT)
    return 20;
  else if (pwm_percent_pump == PWM_PUMP_30_PERCENT)
    return 30;
  else if (pwm_percent_pump == PWM_PUMP_40_PERCENT)
    return 40;
  else if (pwm_percent_pump == PWM_PUMP_50_PERCENT)
    return 50;
  else if (pwm_percent_pump == PWM_PUMP_60_PERCENT)
    return 60;
  else if (pwm_percent_pump == PWM_PUMP_70_PERCENT)
    return 70;
  else if (pwm_percent_pump == PWM_PUMP_80_PERCENT)
    return 80;
  else if (pwm_percent_pump == PWM_PUMP_90_PERCENT)
    return 90;
  else if (pwm_percent_pump == PWM_PUMP_100_PERCENT)
    return 100;
  else
    return 0;
}

void SSRCTRL_set_state_valve(bool enable)
{
  if (enable)
    SSR_VALVE_ON();
  else
    SSR_VALVE_OFF();
}

