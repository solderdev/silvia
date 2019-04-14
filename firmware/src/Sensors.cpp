#include "Sensors.hpp"
#include "Pins.hpp"
#include "Timers.hpp"
#include "TaskConfig.hpp"
#include "coffee_config.hpp"

// static void timer_callback(void);

static SensorsHandler *instance = nullptr;

Sensor::Sensor(adc1_channel_t adc_channel, esp_adc_cal_characteristics_t *adc_chars) :
  value_degc(888.0f),
  adc_channel_(adc_channel),
  adc_chars_(adc_chars),
  avg_buffer_idx_(0)
{
}

SensorsHandler::SensorsHandler() :
  sem_update_(nullptr),
  task_handle_(nullptr),
  timer_update_(nullptr)
{
  if (instance)
  {
    Serial.println("ERROR: more than one SensorsHandlers generated");
    ESP.restart();
    return;
  }
  
  // https://www.esp32.com/viewtopic.php?f=19&t=2881&start=20#p16166
  // https://esp-idf.readthedocs.io/en/latest/api-reference/peripherals/adc.html#adc-calibration
  // ADC2 limited; ADC1 on GPIOs 32 - 39
    
  // ADC preparation
  // ADC capture width is 12Bit
  adc1_config_width(ADC_WIDTH_12Bit);

  // e.g. ADC1 channel 6 is GPIO34, full-scale voltage 3.9V
  adc1_config_channel_atten(Pins::sensor_top, ADC_ATTEN);
  adc1_config_channel_atten(Pins::sensor_side, ADC_ATTEN);
  adc1_config_channel_atten(Pins::sensor_brewhead, ADC_ATTEN);

  // measure ADC Vref (calibration only)
  #if 0
    esp_err_t error = adc2_vref_to_gpio(GPIO_NUM_25);
    Serial.println(String(error));
  #endif

  // setup ADC characteristics
  memset(&adc_chars_, 0, sizeof(adc_chars_));
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_SENSORS, ADC_ATTEN, ADC_WIDTH_BIT_12, ADC_VREF_MEASURED, &adc_chars_);

  if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF)
    Serial.println("ADC cal-type: eFuse Vref");
  else if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP)
    Serial.println("ADC cal-type: Two Point");
  else
    Serial.println("ADC cal-type: Default");
  
  Serial.println("ADC vref " + String(adc_chars_.vref));
  Serial.println("ADC coeff_a " + String(adc_chars_.coeff_a));
  Serial.println("ADC coeff_b " + String(adc_chars_.coeff_b));
  Serial.println("ADC atten " + String(adc_chars_.atten));
  Serial.println("ADC bit_width " + String(adc_chars_.bit_width));

  // create sensors and configre ADC channels
  sensor_top_ = new Sensor(Pins::sensor_top, &adc_chars_);
  sensor_side_ = new Sensor(Pins::sensor_side, &adc_chars_);
  sensor_brewhead_ = new Sensor(Pins::sensor_brewhead, &adc_chars_);

  // sync semaphore
  sem_update_ = xSemaphoreCreateBinary();

  // create task
  BaseType_t rval = xTaskCreate(&SensorsHandler::task_wrapper, "task_sensor", TaskConfig::SensorsHandler_stacksize, this, TaskConfig::SensorsHandler_priority, &task_handle_);

  if (sem_update_ == NULL /*|| timer_update_ == NULL*/ || rval != pdPASS || task_handle_ == NULL)
  {
    Serial.println("SensorsHandler ERROR init failed");
    return;
  }

  instance = this;
}

SensorsHandler* SensorsHandler::getInstance()
{
  if (instance == nullptr)
    Serial.println("SensorsHandler FATAL ERROR: no instance?!");
  return instance;
}

void SensorsHandler::task_wrapper(void *arg)
{
  static_cast<SensorsHandler *>(arg)->task();
}
void SensorsHandler::task()
{
  // call update function rapidly to pre-fill filter-buffer
  for (uint32_t i = 0; i < SENSORS_BUFFER_SIZE; i++)
    update();

  while (1)
  {
    update();
    vTaskDelay(pdMS_TO_TICKS(28));
  }
}

esp_err_t Sensor::update()
{
  uint32_t voltage;

  // get voltages as mV and insert into buffer
  esp_err_t error = esp_adc_cal_get_voltage((adc_channel_t)adc_channel_, adc_chars_, &voltage);
  avg_buffer_[avg_buffer_idx_] = voltage;

  // move (circular-)buffer index to next position
  if (error == ESP_OK)
    avg_buffer_idx_ = (avg_buffer_idx_ + 1) % SENSORS_BUFFER_SIZE;
  else
  {
    value_degc = 999;
    return error;
  }

  uint64_t sum = 0;
  float value;

  // calculate average of buffer
  for (uint32_t i = 0; i < SENSORS_BUFFER_SIZE; i++)
    sum += avg_buffer_[i];
  value = (float)sum / SENSORS_BUFFER_SIZE;

  // convert mV to deg-C
  value = (13.582 - sqrt(13.582 * 13.582 + 4 * 0.00433 * (2230.8 - value) ) ) / (2 * -0.00433) + 30;

  if (value < 10 || value > 150)
    value = 999;

  value_degc = value;
  
  return error;
}

void SensorsHandler::update()
{
  esp_err_t error = ESP_OK;

  error |= sensor_top_->update();
  error |= sensor_side_->update();
  error |= sensor_brewhead_->update();

  if (error != ESP_OK)
    Serial.println("ESP32-ADC failed!");
  
  // Serial.println("New Values: top=" + String(sensor_top_->value_degc) + " side=" + String(sensor_side_->value_degc) + " bh=" + String(sensor_brewhead_->value_degc));
}

float SensorsHandler::getTempBoilerAvg()
{
  if (instance)
    return (instance->sensor_top_->value_degc + instance->sensor_side_->value_degc) / 2;
  else
    return 999.0f;
}

float SensorsHandler::getTempBoilerMax()
{
  if (instance)
  {
    if (instance->sensor_top_->value_degc > instance->sensor_side_->value_degc)
      return instance->sensor_top_->value_degc;
    else
      return instance->sensor_side_->value_degc;
  }
  else
    return 999.0f;
}

float SensorsHandler::getTempBoilerTop()
{
  if (instance)
    return instance->sensor_top_->value_degc;
  else
    return 999.0f;
}

float SensorsHandler::getTempBoilerSide()
{
  if (instance)
    return instance->sensor_side_->value_degc;
  else
    return 999.0f;
}

float SensorsHandler::getTempBrewhead()
{
  if (instance)
    return instance->sensor_brewhead_->value_degc;
  else
    return 999.0f;
}
