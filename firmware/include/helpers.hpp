#pragma once

#include <Arduino.h>

void systime_init();
unsigned long IRAM_ATTR systime_ms();
