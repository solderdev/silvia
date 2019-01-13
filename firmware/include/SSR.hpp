#pragma once

#include <Arduino.h>
#include <atomic>

class SSR
{
public:
  SSR(uint8_t ctrl_pin);
  void enable();
  void disable();
  bool isEnabled();
  void on();
  void off();

protected:
  std::atomic<bool> enabled_;

private:
  uint8_t ctrl_pin_;
};
