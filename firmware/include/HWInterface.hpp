#pragma once

class WaterControl;
class Button;

class HWInterface
{
public:
  HWInterface(WaterControl *water_control);
  HWInterface(HWInterface const&) = delete;
  void operator=(HWInterface const&)  = delete;
  static HWInterface* getInstance();
  void service();
  bool isActive();
  void powerOff();
  void powerOn();

private:
  Button *btn_power;
  Button *sw_coffee;
  Button *sw_water;
  Button *sw_steam;
  WaterControl *water_control_;
  bool power_trigger_available_;
  uint32_t power_state_;  // 0 if off or ms since turn-on

};
