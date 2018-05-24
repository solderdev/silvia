#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "Arduino.h"


#define PID_TASK_STACKSIZE  2000u  // [words]
#define PID_TASK_PRIORITY   5      // idle: 0, main-loop: 1, wifi: 2, pid: 5, sensors: 4

#define PID_MIN_TEMP   10.0f  // [deg-C] minimum allowed temperature
#define PID_MAX_TEMP  139.0f  // [deg-C] maximum allowed temperature

static float kP = 0.0f;
static float kI = 0.0f;
static float kD = 0.0f;
static uint32_t pid_ts = 0;  // [ms] update interval
static float target_temp = 0.0f;  // [deg-C] target boiler temperature
static uint8_t pid_u1 = 0;  // last output value
static float pid_pv1 = 0.0f;  // last process value (temperature)
static float pid_pv2 = 0.0f;  // second to last process value (temperature)

static SemaphoreHandle_t pid_sem_update = NULL;
static TimerHandle_t pid_timer_update = NULL;
static TaskHandle_t pid_task_handle = NULL;


static void pid_timer_cb(TimerHandle_t pxTimer);
static void pid_task(void * pvParameters);


uint8_t PID_setup(float p, float i, float d, uint32_t ts_ms)
{
  // P, I and D parameters
  kP = p;
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

  // create task
  if (xTaskCreate(pid_task, "task_pid", PID_TASK_STACKSIZE, NULL, 5, &pid_task_handle) != pdPASS)
    return 1; // error

  return 0;
}

void PID_start(void)
{
  SENSORS_update();
  pid_pv2 = SENSORS_get_temp_boiler_max();
  pid_pv1 = SENSORS_get_temp_boiler_max();
  pid_u1 = 0;
  
  SSRCTRL_sync();
  if (pid_timer_update != NULL)
    xTimerStart(pid_timer_update, portMAX_DELAY);
  Serial.println("PID controller ON!");
  Serial.println(" P = " + String(kP) + " I = " + String(kI) + " D = " + String(kD));
}

void PID_stop(void)
{
  if (pid_timer_update != NULL)
    xTimerStop(pid_timer_update, portMAX_DELAY);
  Serial.println("PID controller OFF!");
}

void PID_setTargetTemp(float temp)
{
  if (temp > PID_MIN_TEMP && temp < PID_MAX_TEMP)
    target_temp = temp;
  else
    target_temp = PID_MIN_TEMP;
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
  int32_t u, u_limited;
  
  (void)pvParameters;

  //bool test_done = false;
  
  while(1)
  {
    // wait until the new PID-update is almost ready
    // update temperatures shortly before
    vTaskDelay(pdMS_TO_TICKS(pid_ts - 50));
    SENSORS_update();
    
    if (xSemaphoreTake(pid_sem_update, portMAX_DELAY) == pdTRUE)
    {
      // Serial.println("PID running at " + String(millis()));
  
      pv = SENSORS_get_temp_boiler_avg();  // SENSORS_get_temp_boiler_max();
      e = target_temp - pv;
  
      // PID type C
      float p_share = kP * (pid_pv1 - pv);
      float i_share = kI * ((float)(pid_ts)/1000.0f) * e;
      float d_share = (kD * (2*pid_pv1 - pv - pid_pv2)) / ((float)(pid_ts)/1000.0f);
      u = lroundf(pid_u1 + p_share + i_share + d_share);
      
  //    u = lroundf(
  //        pid_u1
  //        - kP * (pv - pid_pv1)
  //        + kI * pid_ts * e 
  //        - (kD * (pv - 2*pid_pv1 + pid_pv2)) / (float)(pid_ts));
  
      u_limited = u;

      // faster heat-up
      if (e > 20)
        u_limited = 100;

      if (SSRCTRL_get_pwm_pump() == 0)
      {
        if (u_limited > 5 && (pv >= target_temp || fabsf(e) < 1))
          u_limited = 5;
        if (u_limited > 10 && fabsf(e) < 4)
          u_limited = 10;
      }
      // else if (SSRCTRL_get_pwm_pump() == 100 && u_limited < 40)
      // {
      //   u_limited = 40;
      // }
      
      if (u_limited < 0)
        u_limited = 0;
      else if (u_limited > 100)
        u_limited = 100;
        
      SSRCTRL_set_pwm_heater(u_limited);
  
      // for ziegler-nicholds
  //    if (target_temp < 100)
  //      SSRCTRL_set_pwm_heater(1);
  //    else
  //      SSRCTRL_set_pwm_heater(3);
  
      // thermostat
  //    if (pv < target_temp)
  //      SSRCTRL_set_pwm_heater(100);
  //    else
  //      SSRCTRL_set_pwm_heater(0);
  
      // save old values
      pid_pv2 = pid_pv1;
      pid_pv1 = pv;
      pid_u1 = u_limited;
  
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
  
  //    // step response
  //    if (!test_done && temp < 100)
  //      SSRCTRL_set_pwm_heater(100);
  //    else
  //    {
  //      SSRCTRL_set_pwm_heater(0);
  //      test_done = true;
  //    }
    }
    else
    {
      
    }
  }
}

