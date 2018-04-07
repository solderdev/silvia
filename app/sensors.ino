#include "esp_adc_cal.h"
#include "driver/gpio.h"
#include "driver/adc.h"

#define ADC_VREF_MEASURED  1141 // mV

#define PIN_BOILER_SIDE ADC1_CHANNEL_0
#define PIN_BOILER_TOP  ADC1_CHANNEL_1
#define PIN_BREWHEAD    ADC1_CHANNEL_2

#define UPDATE_INTERVAL 1000  // [ms]

esp_adc_cal_characteristics_t adc_chars;

float val_degc_boiler_side = 0.0f;
float val_degc_boiler_top = 0.0f;
float val_degc_brewhead = 0.0f;


void setup_sensors(void)
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
}

void update_sensors(void)
{
  static uint32_t last_update = 0;

  if (millis() - last_update > UPDATE_INTERVAL)
  {
    last_update = millis();
    
    //Serial.println(String(sensorValue) + " mV  " + String(temp) + " degC");
    val_degc_boiler_side = (13.582 - sqrt(13.582 * 13.582 + 4 * 0.00433 * (2230.8 - adc1_to_voltage(PIN_BOILER_SIDE, &adc_chars)) ) ) / (2 * -0.00433) + 30;
    val_degc_boiler_top  = (13.582 - sqrt(13.582 * 13.582 + 4 * 0.00433 * (2230.8 - adc1_to_voltage(PIN_BOILER_TOP, &adc_chars)) ) ) / (2 * -0.00433) + 30;
    val_degc_brewhead    = (13.582 - sqrt(13.582 * 13.582 + 4 * 0.00433 * (2230.8 - adc1_to_voltage(PIN_BREWHEAD, &adc_chars)) ) ) / (2 * -0.00433) + 30;

    //Serial.println("Boiler side: " + String(val_degc_boiler_side) + "C / Boiler top " + String(val_degc_boiler_top) + "C / Brewhead " + String(val_degc_brewhead) + "C");
  }
}

float get_sensor_boiler_side(void)
{
  return val_degc_boiler_side;
}

float get_sensor_boiler_top(void)
{
  return val_degc_boiler_top;
}

float get_sensor_brewhead(void)
{
  return val_degc_brewhead;
}

