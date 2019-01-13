#pragma once

#include <Arduino.h>
#include <driver/adc.h>

class Pins
{
public:
  static constexpr uint32_t led_green = 25;

  static constexpr uint32_t button_power = 23;

  static constexpr uint32_t switch_coffee = 17;
  static constexpr uint32_t switch_water  = 16;
  static constexpr uint32_t switch_steam = 4;

  static constexpr uint32_t ssr_pump = 27;
  static constexpr uint32_t ssr_heater = 26;
  static constexpr uint32_t ssr_valve = 33;

  static constexpr adc1_channel_t sensor_side = ADC1_CHANNEL_6;
  static constexpr adc1_channel_t sensor_top = ADC1_CHANNEL_0;
  static constexpr adc1_channel_t sensor_brewhead = ADC1_CHANNEL_3;
};
