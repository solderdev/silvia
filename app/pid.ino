#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "Arduino.h"


#define PID_TASK_STACKSIZE  500u  // words
#define PID_TASK_PRIORITY   5     // idle is 0, main loop is 1, wifi is 2

#define PID_MAX_TEMP  139  // deg-C

static float kP = 0.0f;
static float kI = 0.0f;
static float kD = 0.0f;
static uint32_t ts = 0;  // [ms]
static SemaphoreHandle_t sem_update = NULL;
//static StaticSemaphore_t sem_update_buffer;
static TimerHandle_t timer_update = NULL;
static TaskHandle_t task_handle = NULL;


static void pid_timer_cb(TimerHandle_t pxTimer);
static void pid_task(void * pvParameters);


uint8_t pid_setup(float p, float i, float d, uint32_t ts_ms)
{
  // P, I and D parameters
  kP = p;
  kI = i;
  kD = d;

  // sampling time
  ts = ts_ms;

  // sync semaphore
//  sem_update = xSemaphoreCreateBinaryStatic(&sem_update_buffer);
  sem_update = xSemaphoreCreateBinary();
  if (sem_update == NULL)
    return 1; // error

  // update timer
  timer_update = xTimerCreate("tmr_pid", pdMS_TO_TICKS(ts_ms), pdTRUE, NULL, pid_timer_cb);
  if (timer_update == NULL)
    return 1; // error

  // create task
  if (xTaskCreate(pid_task, "task_pid", PID_TASK_STACKSIZE, NULL, 5, &task_handle) != pdPASS)
    return 1; // error

  return 0;
}

static void pid_timer_cb(TimerHandle_t pxTimer)
{
  (void)pxTimer;
  xSemaphoreGive(sem_update);
}

void pid_task(void * pvParameters)
{
  while(1)
  {
    xSemaphoreTake(sem_update, portMAX_DELAY);
    Serial.println("PID running at " + String(millis()));
  }
}

//void service_pid(void)
//{
//  static uint32_t last_update = 0;
//  uint32_t now = millis();
//  
//  if (ts != 0 && now - last_update > ts)
//  {
//    last_update = now;
//
//    // TODO
//  }
//}

