#include <Arduino.h>
#include "Button.hpp"

#define DEBOUNCE_INTERVAL_BTN  11  // [ms]
#define DEBOUNCE_COUNT_BTN     4

Button::Button(uint8_t pin, uint8_t mode) : 
  pin_(pin),
  debounce_(0)
{
  pinMode(pin, mode);
  
  timer_debounce_ = xTimerCreate("tmr_btn", pdMS_TO_TICKS(DEBOUNCE_INTERVAL_BTN), pdTRUE, this, [](TimerHandle_t pxTimer) {
    Button *btn = static_cast<Button *>(pvTimerGetTimerID(pxTimer));
    // get pin state
    // increase counter up to DEBOUNCE_COUNT_BTN if pin is asserted
    if (!digitalRead(btn->pin_))
      btn->debounce_ = (btn->debounce_ + 1 > DEBOUNCE_COUNT_BTN) ? DEBOUNCE_COUNT_BTN : btn->debounce_ + 1;
    else
      btn->debounce_ = 0;
  });

  if (timer_debounce_ != NULL)
    xTimerStart(timer_debounce_, portMAX_DELAY);
}

bool Button::active()
{
  if (debounce_ == DEBOUNCE_COUNT_BTN)
    return true;
  else
    return false;
}
