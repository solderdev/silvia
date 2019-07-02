#include "helpers.hpp"
#include <Arduino.h>


static uint32_t systime_multiplier = 1;

void systime_init()
{
  systime_multiplier = 80000000 / getApbFrequency();
}

unsigned long IRAM_ATTR systime_ms()
{
  return (unsigned long) ((esp_timer_get_time() * systime_multiplier) / 1000ULL);
}
