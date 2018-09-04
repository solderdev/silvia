#include "esp_adc_cal.h"
#include "driver/gpio.h"
#include "driver/adc.h"

#define ADC_VREF_MEASURED  1141       // [mV]

#define SENSOR_TASK_STACKSIZE  1000u  // [words]
#define SENSOR_TASK_PRIORITY   4      // idle: 0, main-loop: 1, wifi: 2, pid: 5, sensors: 4
#define SENSOR_UPDATE_RATE     100    // [ms]

#define ADC_SENSORS      ADC_UNIT_1
#define PIN_BOILER_SIDE  ADC_CHANNEL_6
#define PIN_BOILER_TOP   ADC_CHANNEL_0
#define PIN_BREWHEAD     ADC_CHANNEL_3

esp_adc_cal_characteristics_t adc_chars;

float val_degc_boiler_side = 888.0f;
float val_degc_boiler_top = 888.0f;
float val_degc_brewhead = 888.0f;

// circular buffer for running average
#define SENSORS_BUFFER_SIZE  12  // 12 values, 40ms apart --> running average over 480ms
uint32_t sensor_side_buffer[SENSORS_BUFFER_SIZE];
uint32_t sensor_top_buffer[SENSORS_BUFFER_SIZE];
uint32_t sensor_brewhead_buffer[SENSORS_BUFFER_SIZE];
uint8_t sensor_buffer_idx = 0;  // current writing index

static SemaphoreHandle_t sensor_sem_update = NULL;
//static TimerHandle_t sensor_timer_update = NULL;
static TaskHandle_t sensor_task_handle = NULL;

static hw_timer_t * timer_sensor_update;


//static void sensor_timer_cb(TimerHandle_t pxTimer);
static void sensor_task(void * pvParameters);
static void SENSORS_update(void);
static void IRAM_ATTR sensor_timer_cb(void);


void SENSORS_setup(void)
{
  // https://www.esp32.com/viewtopic.php?f=19&t=2881&start=20#p16166
  // https://esp-idf.readthedocs.io/en/latest/api-reference/peripherals/adc.html#adc-calibration
  // ADC2 limited
  // ADC1 on GPIOs 32 - 39
    
  // ADC preparation
  // ADC capture width is 12Bit
  adc1_config_width(ADC_WIDTH_12Bit);
  // ADC1 channel 6 is GPIO34, full-scale voltage 3.9V
  adc1_config_channel_atten((adc1_channel_t)(PIN_BOILER_SIDE), ADC_ATTEN_11db);
  adc1_config_channel_atten((adc1_channel_t)(PIN_BOILER_TOP), ADC_ATTEN_11db);
  adc1_config_channel_atten((adc1_channel_t)(PIN_BREWHEAD), ADC_ATTEN_11db);
  
//  // measure ADC Vref
//  esp_err_t error = adc2_vref_to_gpio(GPIO_NUM_25);
//  Serial.println(String(error));

  // setup ADC characteristics
  memset(&adc_chars, 0, sizeof(adc_chars));
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_SENSORS, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, ADC_VREF_MEASURED, &adc_chars);
  
  if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF)
    Serial.println("ADC cal-type: eFuse Vref");
  else if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP)
    Serial.println("ADC cal-type: Two Point");
  else
    Serial.println("ADC cal-type: Default");
  
  Serial.println("ADC vref " + String(adc_chars.vref));
  Serial.println("ADC coeff_a " + String(adc_chars.coeff_a));
  Serial.println("ADC coeff_b " + String(adc_chars.coeff_b));
  Serial.println("ADC atten " + String(adc_chars.atten));
  Serial.println("ADC bit_width " + String(adc_chars.bit_width));

  // sync semaphore
  sensor_sem_update = xSemaphoreCreateBinary();
  if (sensor_sem_update == NULL)
    return; // error
    
//  // update timer
//  sensor_timer_update = xTimerCreate("tmr_sensor", pdMS_TO_TICKS(SENSOR_UPDATE_RATE), pdTRUE, NULL, sensor_timer_cb);
//  if (sensor_timer_update == NULL)
//    return; // error
//  xTimerStart(sensor_timer_update, portMAX_DELAY);

  // create task
  if (xTaskCreate(sensor_task, "task_sensor", SENSOR_TASK_STACKSIZE, NULL, SENSOR_TASK_PRIORITY, &sensor_task_handle) != pdPASS)
    return; // error

  timer_sensor_update = timerBegin(2, 80, true);  // timer 2
  if (timer_sensor_update == NULL)
    return;
  timerAttachInterrupt(timer_sensor_update, &sensor_timer_cb, true);
  // update: 25Hz -> 40ms
  timerAlarmWrite(timer_sensor_update, 40000, true);
}

//static void sensor_timer_cb(TimerHandle_t pxTimer)
//{
//  (void)pxTimer;
//  xSemaphoreGive(sensor_sem_update);
//}

static void sensor_task(void * pvParameters)
{
  // call update function rapidly to pre-fill filter-buffer
  for (uint32_t i = 0; i < SENSORS_BUFFER_SIZE; i++)
    SENSORS_update();

  // start timer
  timerAlarmEnable(timer_sensor_update);

  while(1)
  {
    xSemaphoreTake(sensor_sem_update, portMAX_DELAY);
    // called every 40ms by timer below
    SENSORS_update();
  }
}

static void IRAM_ATTR sensor_timer_cb(void)
{
  xSemaphoreGive(sensor_sem_update);
}

static void SENSORS_update(void)
{
  float val_mV_boiler_side, val_mV_boiler_top, val_mV_brewhead;
  uint32_t voltage;
  uint64_t sum_side = 0, sum_top = 0, sum_brewhead = 0;
  esp_err_t error = ESP_OK;

  // get voltages as mV and insert into buffer
  error |= esp_adc_cal_get_voltage(PIN_BOILER_SIDE, &adc_chars, &voltage);
  sensor_side_buffer[sensor_buffer_idx] = voltage;
  error |= esp_adc_cal_get_voltage(PIN_BOILER_TOP, &adc_chars, &voltage);
  sensor_top_buffer[sensor_buffer_idx] = voltage;
  error |= esp_adc_cal_get_voltage(PIN_BREWHEAD, &adc_chars, &voltage);
  sensor_brewhead_buffer[sensor_buffer_idx] = voltage;

  if (error != ESP_OK)
  {
    Serial.println("ESP32-ADC failed!");
    return;
  }

  // move (circular-)buffer index to next position
  sensor_buffer_idx = (sensor_buffer_idx + 1) % SENSORS_BUFFER_SIZE;

  // calculate average of buffer
  for (uint32_t i = 0; i < SENSORS_BUFFER_SIZE; i++)
  {
    sum_side += sensor_side_buffer[i];
    sum_top += sensor_top_buffer[i];
    sum_brewhead += sensor_brewhead_buffer[i];
  }
  val_mV_boiler_side = (float)sum_side / SENSORS_BUFFER_SIZE;
  val_mV_boiler_top = (float)sum_top / SENSORS_BUFFER_SIZE;
  val_mV_brewhead = (float)sum_brewhead / SENSORS_BUFFER_SIZE;

  // raise task priority to avoid interruption
  vTaskPrioritySet(NULL, configMAX_PRIORITIES - 1);

  // convert mV to deg-C
  val_degc_boiler_side = (13.582 - sqrt(13.582 * 13.582 + 4 * 0.00433 * (2230.8 - val_mV_boiler_side) ) ) / (2 * -0.00433) + 30;
  val_degc_boiler_top  = (13.582 - sqrt(13.582 * 13.582 + 4 * 0.00433 * (2230.8 - val_mV_boiler_top) ) ) / (2 * -0.00433) + 30;
  val_degc_brewhead    = (13.582 - sqrt(13.582 * 13.582 + 4 * 0.00433 * (2230.8 - val_mV_brewhead) ) ) / (2 * -0.00433) + 30;

  // make sure values are in plausible ranges
  if (val_degc_boiler_side < 10 || val_degc_boiler_side > 150)
    val_degc_boiler_side = 999;
  if (val_degc_boiler_top < 10 || val_degc_boiler_top > 150)
    val_degc_boiler_top = 999;
  if (val_degc_brewhead < 10 || val_degc_brewhead > 150)
    val_degc_brewhead = 999;

  // re-set normal task priority
  vTaskPrioritySet(NULL, SENSOR_TASK_PRIORITY);
}

float SENSORS_get_temp_boiler_side(void)
{
  return val_degc_boiler_side;
}

float SENSORS_get_temp_boiler_top(void)
{
  return val_degc_boiler_top;
}

float SENSORS_get_temp_boiler_max(void)
{
  if (val_degc_boiler_side > val_degc_boiler_top)
    return val_degc_boiler_side;
  else
    return val_degc_boiler_top;
}

float SENSORS_get_temp_boiler_avg(void)
{
  return (val_degc_boiler_side + val_degc_boiler_top) / 2;
}

float SENSORS_get_temp_brewhead(void)
{
  return val_degc_brewhead;
}
