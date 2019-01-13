#pragma once

#include <Arduino.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <atomic>

#define SENSORS_BUFFER_SIZE  24

class Sensor
{
public:
  Sensor(adc1_channel_t adc_channel, esp_adc_cal_characteristics_t *adc_chars);
  esp_err_t update();

  std::atomic<float> value_degc;

private:
  adc1_channel_t adc_channel_;
  esp_adc_cal_characteristics_t *adc_chars_;
  uint32_t avg_buffer_[SENSORS_BUFFER_SIZE];
  uint8_t avg_buffer_idx_;  // current writing index
};


class SensorsHandler
{
public:
  SensorsHandler();
  SensorsHandler(SensorsHandler const&) = delete;
  void operator=(SensorsHandler const&)  = delete;
  static SensorsHandler* getInstance();
  void triggerUpdate();
  static float getTempBoilerAvg();
  static float getTempBoilerMax();
  static float getTempBoilerTop();
  static float getTempBoilerSide();
  static float getTempBrewhead();

  static constexpr int32_t ADC_VREF_MEASURED = 1141;  // mV
  static constexpr adc_unit_t ADC_SENSORS    = ADC_UNIT_1;
  static constexpr adc_atten_t ADC_ATTEN     = ADC_ATTEN_11db;

private:
  static void timer_cb_wrapper(TimerHandle_t arg);
  void timer_cb();
  static void task_wrapper(void *arg);
  void task();
  void update();

  Sensor *sensor_top_;
  Sensor *sensor_side_;
  Sensor *sensor_brewhead_;
  esp_adc_cal_characteristics_t adc_chars_;
  SemaphoreHandle_t sem_update_;
  TaskHandle_t task_handle_;
  hw_timer_t *timer_update_;

  static constexpr uint32_t UPDATE_PERIOD_MS = 29;  // ms
};
