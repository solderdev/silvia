#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "Arduino.h"
#include "pid.h"


#define PID_TASK_STACKSIZE  4000u  // [words]
#define PID_TASK_PRIORITY   5      // idle: 0, main-loop: 1, wifi: 2, pid: 5, sensors: 4

#define PID_MIN_TEMP   10.0f  // [deg-C] minimum allowed temperature
#define PID_MAX_TEMP  139.0f  // [deg-C] maximum allowed temperature

static float kPpos = 0.0f;  // positive contribution, if PV_old > PV
static float kPneg = 0.0f;  // negative contribution, if PV_old < PV
static float kI = 0.0f;
static float kD = 0.0f;
static uint32_t pid_ts = 0;  // [ms] update interval
static float target_temp = 0.0f;  // [deg-C] target boiler temperature
static PID_Mode_t target_mode = PID_MODE_WATER;
static float pid_u1 = 0.0f;   // last output value
static float pid_pv1 = 0.0f;  // last process value (temperature)
static float pid_pv2 = 0.0f;  // second to last process value (temperature)
#define PID_OVERRIDE_CNT  3  // how many times is PID overruled
static float pid_u_override = -1.0f;  // override next PID iteration with this value if positive
static int8_t pid_u_override_cnt = 0;  // counter for how many PID cycles the override should be in place
static bool pid_enabled = false;

static SemaphoreHandle_t pid_sem_update = NULL;
static TimerHandle_t pid_timer_update = NULL;
static TaskHandle_t pid_task_handle = NULL;


static void pid_timer_cb(TimerHandle_t pxTimer);
static void pid_task(void * pvParameters);


uint8_t PID_setup(float p_pos, float p_neg, float i, float d, uint32_t ts_ms)
{
  // P, I and D parameters
  kPpos = p_pos;
  kPneg = p_neg;
  kI = i;
  kD = d;

  // sampling time
  pid_ts = ts_ms;

  target_temp = PID_MIN_TEMP;

  // sync semaphore
  pid_sem_update = xSemaphoreCreateBinary();
  if (pid_sem_update == NULL)
    return 1; // error

  // update timer - start must be called separately
  pid_timer_update = xTimerCreate("tmr_pid", pdMS_TO_TICKS(pid_ts), pdTRUE, NULL, pid_timer_cb);
  if (pid_timer_update == NULL)
    return 1; // error
  
  xTimerStart(pid_timer_update, portMAX_DELAY);

  // create task
  if (xTaskCreate(pid_task, "task_pid", PID_TASK_STACKSIZE, NULL, PID_TASK_PRIORITY, &pid_task_handle) != pdPASS)
    return 1; // error

  return 0;
}

void PID_start(void)
{
  pid_pv2 = SENSORS_get_temp_boiler_avg();
  pid_pv1 = SENSORS_get_temp_boiler_avg();
  pid_u1 = 0.0f;
  
  SSRCTRL_sync();
  xTimerReset(pid_timer_update, 0);
  
  WIFI_resetBuffer();

  pid_enabled = true;
  Serial.println("PID controller ON!");
  Serial.println("P+ = " + String(kPpos) + "P- = " + String(kPneg) + " I = " + String(kI) + " D = " + String(kD));
}

void PID_stop(void)
{
  pid_enabled = false;
  SSRCTRL_set_pwm_heater(0);
  Serial.println("PID controller OFF!");
}

void PID_override(float u_override)
{
  pid_u_override = u_override;
  pid_u_override_cnt = PID_OVERRIDE_CNT;
}

void PID_setTargetTemp(float temp, PID_Mode_t mode)
{
  if (temp > PID_MIN_TEMP && temp < PID_MAX_TEMP)
    target_temp = temp;
  else
    target_temp = PID_MIN_TEMP;

  target_mode = mode;
}

float PID_getTargetTemp(void)
{
  return target_temp;
}

static void pid_timer_cb(TimerHandle_t pxTimer)
{
  (void)pxTimer;
  xSemaphoreGive(pid_sem_update);
}

static void pid_task(void * pvParameters)
{
  float pv, e;
  float u, u_limited;
  float p_share, i_share, d_share;
  //bool test_done = false;
  (void)pvParameters;

  while(1)
  {
    if (xSemaphoreTake(pid_sem_update, portMAX_DELAY) == pdTRUE)
    {
      if (pid_enabled == true)
      {
        // Serial.println("PID running at " + String(millis()));
        if (target_mode == PID_MODE_WATER)
          pv = SENSORS_get_temp_boiler_avg();
        else
          pv = SENSORS_get_temp_boiler_max();
          
        e = target_temp - pv;
//        float e1 = target_temp - pid_pv1;  // for type A and B

        // from Wikipedia (same as type A)
//        float e2 = target_temp - pid_pv2;
//        p_share = kP * (e - e1);
//        i_share = kI * ((float)(pid_ts)/1000.0f) * e;
//        d_share = (kD * (e - 2*e1 + e2)) / ((float)(pid_ts)/1000.0f);

        // PID type B
//        p_share = kP * (e - e1);
//        i_share = kI * ((float)(pid_ts)/1000.0f) * e;
//        d_share = (kD * (2*pid_pv1 - pv - pid_pv2)) / ((float)(pid_ts)/1000.0f);
        
        // PID type C
        if ((pid_pv1 - pv) > 0)
          p_share = kPpos * (pid_pv1 - pv);
        else
          p_share = kPneg * (pid_pv1 - pv);
        i_share = kI * ((float)(pid_ts)/1000.0f) * e;
        d_share = (kD * (2*pid_pv1 - pv - pid_pv2)) / ((float)(pid_ts)/1000.0f);
        
        u = pid_u1 + p_share + i_share + d_share;
    
        u_limited = u;

#if 1
        // faster heat-up, if far too cold (15*C)
        if (e > 15)
          u_limited = 100;

        if (SSRCTRL_get_pwm_pump() == 0)
        {
          // limit heater, if pump is off and we are hotter than SP
          if (u_limited > 5 && pv >= target_temp + 0.5)
            u_limited = 5;
            
//          if (u_limited > 10 && fabsf(e) < 1)
//            u_limited = 10;
//          if (u_limited > 15 && fabsf(e) < 4)
//            u_limited = 15;
        }
        else if (SSRCTRL_get_pwm_pump() == 100 && u_limited < 20 && e > 0)
        {
          // set a minimum of 20% heater during a shot, when temp is too low
          //u_limited = 20;
        }

        // apply override value if activated
        if (pid_u_override >= 0.0f && pid_u_override_cnt > 0)
        {
          u_limited = pid_u_override;
          pid_u_override_cnt--;
        }
#endif

        // safety
        if (u_limited < 0)
          u_limited = 0;
        else if (u_limited > 100)
          u_limited = 100;

        // check again, if we got interrupted (not perfect, but better)
        if (pid_enabled == true)
          SSRCTRL_set_pwm_heater(lroundf(u_limited));
    
        // thermostat
    //    if (pv < target_temp)
    //      SSRCTRL_set_pwm_heater(100);
    //    else
    //      SSRCTRL_set_pwm_heater(0);
    
        // save old values
        pid_pv2 = pid_pv1;
        pid_pv1 = pv;
        pid_u1 = u_limited;
      }  // end of pid_enabled
      else
      {
        p_share = 0;
        i_share = 0;
        d_share = 0;
        u = 0;
      }

      if (1)
        Serial.println(String(millis()) + " , " + 
                       String(target_temp) + " , " + 
                       String(SENSORS_get_temp_boiler_side()) + " , " + 
                       String(SENSORS_get_temp_boiler_top()) + " , " + 
                       String(SENSORS_get_temp_brewhead()) + " , " + 
                       String(SSRCTRL_get_pwm_heater()) + " , " + 
                       String(SSRCTRL_get_pwm_pump()) + " , " + 
                       String(u) + " , " + 
                       String(p_share) + " , " + 
                       String(i_share) + " , " + 
                       String(d_share) );

      WIFI_addToBuffer(SENSORS_get_temp_boiler_top(), 
                       SENSORS_get_temp_boiler_side(), 
                       SENSORS_get_temp_brewhead(), 
                       SSRCTRL_get_pwm_heater(),
                       SSRCTRL_get_pwm_pump(),
                       p_share,
                       i_share,
                       d_share,
                       u);
                       
      WIFI_updateInfluxDB();
      
  //    // step response
  //    if (!test_done && temp < 100)
  //      SSRCTRL_set_pwm_heater(100);
  //    else
  //    {
  //      SSRCTRL_set_pwm_heater(0);
  //      test_done = true;
  //    }
    }  // end of sem take
  }  // end of while(1)
}
