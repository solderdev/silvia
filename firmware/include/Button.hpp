#pragma once

#include "FreeRTOS.h"
#include "freertos/timers.h"
#include <atomic>

class Button
{
public:
  Button(uint8_t pin, uint8_t mode);
  bool active();

private:
  uint8_t pin_;
  std::atomic<uint8_t> debounce_;
  TimerHandle_t timer_debounce_;
};
