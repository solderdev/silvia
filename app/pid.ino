#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "Arduino.h"


#define PID_TASK_STACKSIZE  1000u  // [words]
#define PID_TASK_PRIORITY   5      // idle: 0, main-loop: 1, wifi: 2, pid: 5, sensors: 4

#define PID_MIN_TEMP   10.0f  // [deg-C] minimum allowed temperature
#define PID_MAX_TEMP  139.0f  // [deg-C] maximum allowed temperature

static float kP = 0.0f;
static float kI = 0.0f;
static float kD = 0.0f;
static uint32_t ts = 0;  // [ms] update interval
static float target_temp = 0.0f;  // [deg-C] target boiler temperature
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
  ts = ts_ms;

  target_temp = PID_MIN_TEMP;

  // sync semaphore
  pid_sem_update = xSemaphoreCreateBinary();
  if (pid_sem_update == NULL)
    return 1; // error

  // update timer - start must be called separately
  pid_timer_update = xTimerCreate("tmr_pid", pdMS_TO_TICKS(ts_ms), pdTRUE, NULL, pid_timer_cb);
  if (pid_timer_update == NULL)
    return 1; // error

  // create task
  if (xTaskCreate(pid_task, "task_pid", PID_TASK_STACKSIZE, NULL, 5, &pid_task_handle) != pdPASS)
    return 1; // error

  return 0;
}

void PID_start(void)
{
  if (pid_timer_update != NULL)
    xTimerStart(pid_timer_update, portMAX_DELAY);
  Serial.println("PID controller ON!");
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

static void pid_timer_cb(TimerHandle_t pxTimer)
{
  (void)pxTimer;
  xSemaphoreGive(pid_sem_update);
}

static void pid_task(void * pvParameters)
{
  (void)pvParameters;
  
  while(1)
  {
    float temp = 0.0f;
    xSemaphoreTake(pid_sem_update, portMAX_DELAY);
    // Serial.println("PID running at " + String(millis()));

    temp = SENSORS_get_temp_boiler_side();

    // TODO

    if (temp >= target_temp)
      SSRCTRL_set_pwm_heater(0);

    if (temp < target_temp)
      SSRCTRL_set_pwm_heater(100);
  }
}

