#include "SSR.hpp"

SSR::SSR(uint8_t ctrl_pin) :
  enabled_(false),
  ctrl_pin_(ctrl_pin)
{
  disable();
}

void SSR::enable()
{
  off();
  pinMode(ctrl_pin_, OUTPUT);
  enabled_ = true;
}

void SSR::disable()
{
  off();
  pinMode(ctrl_pin_, INPUT);
  enabled_ = false;
}

bool IRAM_ATTR SSR::isEnabled()
{
  return enabled_;
}

void IRAM_ATTR SSR::on()
{
  if (enabled_)
    digitalWrite(ctrl_pin_, HIGH);
}

void IRAM_ATTR SSR::off()
{
  digitalWrite(ctrl_pin_, LOW);
}