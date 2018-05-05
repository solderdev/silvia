#include "esp_adc_cal.h"
#include "driver/gpio.h"
#include "driver/adc.h"

#define ADC_VREF_MEASURED  1141       // [mV]

#define SENSOR_TASK_STACKSIZE  1000u  // [words]
#define SENSOR_TASK_PRIORITY   4      // idle: 0, main-loop: 1, wifi: 2, pid: 5, sensors: 4
#define SENSOR_UPDATE_RATE     100    // [ms]

#define PIN_BOILER_SIDE  ADC1_CHANNEL_6
#define PIN_BOILER_TOP   ADC1_CHANNEL_0
#define PIN_BREWHEAD     ADC1_CHANNEL_3

esp_adc_cal_characteristics_t adc_chars;

float val_degc_boiler_side = 888.0f;
float val_degc_boiler_top = 888.0f;
float val_degc_brewhead = 888.0f;

static SemaphoreHandle_t sensor_sem_update = NULL;
static TimerHandle_t sensor_timer_update = NULL;
static TaskHandle_t sensor_task_handle = NULL;


static void sensor_timer_cb(TimerHandle_t pxTimer);
static void sensor_task(void * pvParameters);
static void SENSORS_update(void);


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
  adc1_config_channel_atten(PIN_BOILER_SIDE, ADC_ATTEN_11db);
  adc1_config_channel_atten(PIN_BOILER_TOP, ADC_ATTEN_11db);
  adc1_config_channel_atten(PIN_BREWHEAD, ADC_ATTEN_11db);
  
//  // measure ADC Vref
//  esp_err_t error = adc2_vref_to_gpio(GPIO_NUM_25);
//  Serial.println(String(error));

  // setup ADC characteristics
  esp_adc_cal_get_characteristics(ADC_VREF_MEASURED, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, &adc_chars);
  Serial.println("ADC v_ref " + String(adc_chars.v_ref));
  Serial.println("ADC gain " + String(adc_chars.gain));
  Serial.println("ADC offset " + String(adc_chars.offset));
  Serial.println("ADC ideal_offset " + String(adc_chars.ideal_offset));
  Serial.println("ADC bit_width " + String(adc_chars.bit_width));

  // sync semaphore
  sensor_sem_update = xSemaphoreCreateBinary();
  if (sensor_sem_update == NULL)
    return; // error
    
  // update timer
  sensor_timer_update = xTimerCreate("tmr_sensor", pdMS_TO_TICKS(SENSOR_UPDATE_RATE), pdTRUE, NULL, sensor_timer_cb);
  if (sensor_timer_update == NULL)
    return; // error
  xTimerStart(sensor_timer_update, portMAX_DELAY);

  // create task
  if (xTaskCreate(sensor_task, "task_sensor", SENSOR_TASK_STACKSIZE, NULL, 5, &sensor_task_handle) != pdPASS)
    return; // error
}

static void sensor_timer_cb(TimerHandle_t pxTimer)
{
  (void)pxTimer;
  xSemaphoreGive(sensor_sem_update);
}

static void sensor_task(void * pvParameters)
{ 
  while(1)
  {
    xSemaphoreTake(sensor_sem_update, portMAX_DELAY);

    SENSORS_update();
  }
}

static void SENSORS_update(void)
{
  uint32_t val_mV_boiler_side = adc1_to_voltage(PIN_BOILER_SIDE, &adc_chars);
  uint32_t val_mV_boiler_top = adc1_to_voltage(PIN_BOILER_TOP, &adc_chars);
  uint32_t val_mV_brewhead = adc1_to_voltage(PIN_BREWHEAD, &adc_chars);
  
  val_degc_boiler_side = (13.582 - sqrt(13.582 * 13.582 + 4 * 0.00433 * (2230.8 - val_mV_boiler_side) ) ) / (2 * -0.00433) + 30;
  val_degc_boiler_top  = (13.582 - sqrt(13.582 * 13.582 + 4 * 0.00433 * (2230.8 - val_mV_boiler_top) ) ) / (2 * -0.00433) + 30;
  val_degc_brewhead    = (13.582 - sqrt(13.582 * 13.582 + 4 * 0.00433 * (2230.8 - val_mV_brewhead) ) ) / (2 * -0.00433) + 30;

//  Serial.println("Boiler side: " + String(val_mV_boiler_side) + "mV / Boiler top " + String(val_mV_boiler_top) + "mV / Brewhead " + String(val_mV_brewhead) + "mV");
//  Serial.println("Boiler side: " + String(val_degc_boiler_side) + "C / Boiler top " + String(val_degc_boiler_top) + "C / Brewhead " + String(val_degc_brewhead) + "C");
                 
  // make sure values are in plausible ranges
  if (val_degc_boiler_side < 10 || val_degc_boiler_side > 150)
    val_degc_boiler_side = 999;
  if (val_degc_boiler_top < 10 || val_degc_boiler_top > 150)
    val_degc_boiler_top = 999;
  if (val_degc_brewhead < 10 || val_degc_brewhead > 150)
    val_degc_brewhead = 999;
}

float SENSORS_get_temp_boiler_side(void)
{
  return val_degc_boiler_side;
}

float SENSORS_get_temp_boiler_top(void)
{
  return val_degc_boiler_top;
}

float SENSORS_get_temp_brewhead(void)
{
  return val_degc_brewhead;
}

